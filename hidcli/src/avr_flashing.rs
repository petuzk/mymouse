//! Implementation of the AVR microcontroller flashing over custom HID-based protocol.
//!
//! The module implements [`BootloaderCommunicator::flash`] method
//! for flashing the AVR microcontroller from an ELF file.

use anyhow::{anyhow, ensure, Context, Result};
use elf::{abi::PT_LOAD, endian::LittleEndian, ElfBytes};
use indicatif::ProgressIterator;
use std::{
    borrow::Cow,
    collections::HashMap,
    fmt::{Debug, Display, Write},
    iter,
};

use crate::communicator::BootloaderCommunicator;

/// Opaque type holding the data to be written to the AVR microcontroller.
/// The data is parsed from an ELF file with [`read_flash_data`].
pub struct FlashData<'a>(Vec<(u16, Cow<'a, target::FlashPageContent>)>);

impl<'a> Display for FlashData<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_fmt(format_args!("{} pages of data", self.0.len()))
    }
}

impl<'a> Debug for FlashData<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("FlashData ")?;
        f.debug_map()
            .entries(self.0.iter().map(|(k, v)| {
                (
                    format!("page_{k}"),
                    v.iter()
                        .fold(String::with_capacity(v.len() * 2), |mut output, b| {
                            let _ = write!(output, "{b:02X}");
                            output
                        }),
                )
            }))
            .finish()
    }
}

/// Read the flash data from the given ELF file content.
pub fn read_flash_data(elf_content: &[u8]) -> Result<FlashData> {
    let mut flash_map = HashMap::new();
    let elf = ElfBytes::<LittleEndian>::minimal_parse(elf_content).context("invalid ELF file")?;

    let segments = elf
        .segments()
        .ok_or_else(|| anyhow!("ELF file does not contain program headers (segments) table"))?;

    for segment in segments.into_iter() {
        // skip all non-LOAD segments (should be none) and those which contain no data in the file
        // (RAM allocation segment, which we don't flash)
        if segment.p_type != PT_LOAD || segment.p_filesz == 0 {
            continue;
        }
        ensure!(
            segment.p_paddr < target::FLASH_SIZE_BYTES as u64,
            "physical address exceeds flash capacity"
        );
        let segment_content =
            &elf_content[segment.p_offset as usize..(segment.p_offset + segment.p_filesz) as usize];

        fill_flash_map(
            &mut flash_map,
            iter_page_content(segment.p_paddr as u16, segment_content),
        );
    }

    let mut flash_map = flash_map.into_iter().collect::<Vec<_>>();
    flash_map.sort();
    Ok(FlashData(flash_map))
}

/// Types and consts related to the target AVR Flash memory
mod target {
    /// Total flash size of the targeted AVR microcontroller.
    ///
    /// ATmega8u2 has 8Kb flash.
    pub const FLASH_SIZE_BYTES: u16 = 8192;

    /// Flash page size of the targeted AVR microcontroller in bytes.
    ///
    /// ATmega8u2 has 128-bytes pages. See `SPM_PAGESIZE` in `iom8u2.h`.
    pub const FLASH_PAGE_SIZE_BYTES: u16 = 128;

    /// Type alias for a fixed-size array representing a single flash page content.
    pub type FlashPageContent = [u8; FLASH_PAGE_SIZE_BYTES as usize];
}

impl BootloaderCommunicator {
    /// Flash the AVR microcontroller.
    pub fn flash(&self, flash_data: &FlashData) -> Result<()> {
        use deku::prelude::*;

        #[derive(Debug, PartialEq, Eq, DekuWrite)]
        #[deku(endian = "little")]
        struct FlashPagePacket<'a> {
            report_id: u8,
            address: u16,
            data: &'a target::FlashPageContent,
        }

        let bar = indicatif::ProgressBar::new(flash_data.0.len() as u64)
            .with_message("Flashing")
            .with_style(
                indicatif::ProgressStyle::default_bar()
                    .template("{msg}... {bar:30} {pos:>3}/{len:3}")
                    .expect("template should be valid"),
            )
            .with_finish(indicatif::ProgressFinish::AndLeave);
        for (page_index, page_content) in flash_data.0.iter().progress_with(bar) {
            self.device.write(
                FlashPagePacket {
                    report_id: 0,
                    address: page_index * target::FLASH_PAGE_SIZE_BYTES,
                    data: page_content,
                }
                .to_bytes()?
                .as_ref(),
            )?;
        }

        Ok(())
    }
}

/// Fill `flash_map` with given page content, overwriting or joining existing data when necessary.
/// First item from the iterator and map key is the page index (e.g. page index 1 equals 128 bytes offset).
fn fill_flash_map<'a, const N: usize>(
    flash_map: &mut HashMap<u16, Cow<'a, [u8; N]>>,
    with: impl Iterator<Item = (u16, PageContent<'a, N>)>,
) {
    for (phys_addr, content) in with {
        match content {
            PageContent::FullPage { data } => {
                flash_map.insert(phys_addr, Cow::Borrowed(data));
            }
            PageContent::PartPage { offset, data } => {
                let page = flash_map
                    .entry(phys_addr)
                    .or_insert_with(|| Cow::Owned([0u8; N]))
                    .to_mut();
                page[offset as usize..offset as usize + data.len()].copy_from_slice(data);
            }
        }
    }
}

/// Split `segment_content` starting at `phys_addr` into [`PageContent`] objects at page boundaries.
///
/// The `phys_addr` is the physical address of the data in bytes. The returned iterator
/// yields pairs of page index (e.g. page index 1 equals 128 bytes offset) and page content.
fn iter_page_content<const N: usize>(
    phys_addr: u16,
    segment_content: &[u8],
) -> impl Iterator<Item = (u16, PageContent<'_, N>)> {
    let first_page_addr = phys_addr / (N as u16);
    let first_page_offset = phys_addr % (N as u16);
    let num_bytes_first_page = (N as u16) - first_page_offset;
    let (first_page_content, remaining_content) = segment_content
        .split_at_checked(num_bytes_first_page as usize)
        .unwrap_or((segment_content, &[])); // if the segment is smaller than num_bytes_first_page
    iter::once((
        first_page_addr,
        PageContent::new(first_page_offset, first_page_content).expect("sizes should be correct"),
    ))
    .chain(iter::zip(
        iter::successors(Some(first_page_addr + 1), |addr| Some(*addr + 1)),
        remaining_content
            .chunks(N)
            .map(|chunk| PageContent::new(0, chunk).expect("sizes should be correct")),
    ))
}

/// A borrowed page content, representing either full page content (an array of size N)
/// or its part (a slice of size less than N at a specific offset).
///
/// This is an intermediate type for creating [`Cow`]s of page content arrays from multiple
/// sources, which might "write" to the same page.
#[derive(Debug, PartialEq, Eq)]
enum PageContent<'a, const N: usize = { target::FLASH_PAGE_SIZE_BYTES as usize }> {
    /// Data takes full page (offset is implicitly 0)
    FullPage { data: &'a [u8; N] },
    /// Data takes part of the page starting at byte `offset`.
    /// It is guaranteed that the slice written at that offset onto the page would fit in it.
    PartPage { offset: u16, data: &'a [u8] },
}

impl<'a, const N: usize> PageContent<'a, N> {
    /// Create a new object from a slice of page content `data` starting at `offset`
    /// within the page of size `N`. Returns [`Err`] with the total size (offset + data length)
    /// if it is larger than `N` (i.e. would not fit in the page).
    fn new(offset: u16, data: &'a [u8]) -> Result<Self, usize> {
        let total_size = offset as usize + data.len();
        if total_size > N {
            Err(total_size)
        } else {
            match data.try_into() {
                Ok(full_page) => Ok(Self::FullPage { data: full_page }),
                Err(_) => Ok(Self::PartPage { offset, data }),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use std::{borrow::Cow, collections::HashMap};

    use super::{fill_flash_map, iter_page_content, PageContent};

    #[test]
    fn test_create_page_content() {
        assert!(matches!(
            dbg!(PageContent::<'_, 4>::new(0, &[1, 2, 3, 4])),
            Ok(PageContent::FullPage { data: [1, 2, 3, 4] })
        ));
        assert!(matches!(
            dbg!(PageContent::<'_, 4>::new(1, &[1, 2])),
            Ok(PageContent::PartPage {
                offset: 1,
                data: [1, 2]
            })
        ));

        assert!(matches!(
            dbg!(PageContent::<'_, 4>::new(0, &[1, 2, 3, 4, 42])),
            Err(5)
        ));
        assert!(matches!(
            dbg!(PageContent::<'_, 4>::new(3, &[1, 2])),
            Err(5)
        ));
        assert!(matches!(
            dbg!(PageContent::<'_, 4>::new(3, &[1, 2, 3, 4])),
            Err(7)
        ));
    }

    #[test]
    fn test_iter_page_content() {
        // simple case: both ends aligned to page boundary
        assert_eq!(
            iter_page_content::<2>(6, &[0, 1, 2, 3]).collect::<Vec<_>>(),
            &[
                (3, PageContent::FullPage { data: &[0, 1] }),
                (4, PageContent::FullPage { data: &[2, 3] })
            ]
        );

        // complex case: both ends not aligned to page boundary
        assert_eq!(
            iter_page_content::<2>(5, &[0, 1, 2, 3]).collect::<Vec<_>>(),
            &[
                (
                    2,
                    PageContent::PartPage {
                        offset: 1,
                        data: &[0]
                    }
                ),
                (3, PageContent::FullPage { data: &[1, 2] }),
                (
                    4,
                    PageContent::PartPage {
                        offset: 0,
                        data: &[3]
                    }
                ),
            ]
        );

        // corner case: segment smaller than page
        assert_eq!(
            iter_page_content::<4>(5, &[1, 2]).collect::<Vec<_>>(),
            &[(
                1,
                PageContent::PartPage {
                    offset: 1,
                    data: &[1, 2]
                }
            )]
        );
    }

    #[test]
    fn test_fill_flash_map() {
        let mut flash_map = HashMap::new();

        // add one full page
        fill_flash_map(
            &mut flash_map,
            vec![(
                2,
                PageContent::FullPage {
                    data: &[1, 2, 3, 4],
                },
            )]
            .into_iter(),
        );

        assert_eq!(flash_map.keys().copied().collect::<Vec<_>>(), &[2]);
        assert!(matches!(
            flash_map.get(&2),
            Some(Cow::Borrowed([1, 2, 3, 4])) // borrowed from the input
        ));

        // overwrite it partially
        fill_flash_map(
            &mut flash_map,
            vec![(
                2,
                PageContent::PartPage {
                    offset: 1,
                    data: &[5, 6],
                },
            )]
            .into_iter(),
        );

        assert_eq!(flash_map.keys().copied().collect::<Vec<_>>(), &[2]);
        assert!(matches!(
            flash_map.get(&2),
            Some(Cow::Owned([1, 5, 6, 4])) // becomes owned due to data change
        ));
    }
}
