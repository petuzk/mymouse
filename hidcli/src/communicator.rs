use anyhow::{anyhow, bail, Result};
use hidapi::{DeviceInfo, HidApi, HidDevice};
use std::{
    collections::HashSet,
    fmt::Debug,
    hash::Hash,
    thread,
    time::{Duration, Instant},
};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum CommunicationError {
    #[error("Timed out while searching for device after mode change")]
    DeviceReSearchTimeout,
}

/// Communicator providing methods for interacting with the application
/// running on the AVR microcontroller.
#[derive(Debug)]
pub struct AppCommunicator {
    device: HidDevice,
}

impl AppCommunicator {
    fn enter_bootloader(&self) -> Result<()> {
        self.device.write(&[0x08, 0x42])?;
        Ok(())
    }
}

impl From<HidDevice> for AppCommunicator {
    fn from(device: HidDevice) -> Self {
        AppCommunicator { device }
    }
}

/// Communicator providing methods for interacting with the bootloader
/// running on the AVR microcontroller.
#[derive(Debug)]
pub struct BootloaderCommunicator {
    pub(crate) device: HidDevice,
}

impl BootloaderCommunicator {
    fn enter_app(&self) -> Result<()> {
        self.device.write(&[0x00, 0xff, 0xff])?;
        Ok(())
    }
}

impl From<HidDevice> for BootloaderCommunicator {
    fn from(device: HidDevice) -> Self {
        BootloaderCommunicator { device }
    }
}

#[derive(Debug)]
pub enum DeviceCommunicator {
    App(AppCommunicator),
    Bootloader(BootloaderCommunicator),
}

#[derive(Debug)]
pub struct Device {
    communicator: DeviceCommunicator,
}

macro_rules! impl_communicator_getter {
    ($desired_mode:ident, $other_mode:ident) => {
        paste::paste! {
            // mutable borrow ensures there's only one user of communicator at a time,
            // and that the mode can't be changed until that communicator is dropped
            pub fn [<$desired_mode:lower _communicator>](&mut self) -> Result<&[<$desired_mode Communicator>]> {
                match self.communicator {
                    DeviceCommunicator::$desired_mode(ref comm) => Ok(comm),
                    DeviceCommunicator::$other_mode(ref comm) => {
                        comm.[<enter_ $desired_mode:lower>]()?;
                        self.communicator = wait_for_comm(Duration::from_secs(5))?;
                        match self.communicator {
                            DeviceCommunicator::$desired_mode(ref comm) => Ok(comm),
                            DeviceCommunicator::$other_mode(_) => Err(anyhow!(
                                concat!("device is still in ", stringify!([<$other_mode:lower>]))
                            )),
                        }
                    }
                }
            }
        }
    };
}

impl Device {
    impl_communicator_getter!(App, Bootloader);
    impl_communicator_getter!(Bootloader, App);

    pub fn current_communicator(&mut self) -> &DeviceCommunicator {
        &self.communicator
    }
}

/// Factory for delayed creation of DeviceCommunicator.
///
/// Because hidapi may return multiple entries for the same physical device, the entries
/// need to be de-duplicated before opening the device (we want to ensure there's only one
/// matching device connected to the host). De-duplication is enabled by implementing Eq and Hash
/// traits, so that this type can be used in a HashSet.
struct CommunicatorBuilder<'a> {
    dev_info: &'a DeviceInfo,
    build_fn: Box<dyn FnOnce(&'a HidApi) -> Result<DeviceCommunicator> + 'a>,
}

const VENDOR_ID: u16 = 0x03EB;
const PRODUCT_ID_APP: u16 = 0x2041;
const PRODUCT_ID_BOOTLOADER: u16 = 0x2067;

impl<'a> CommunicatorBuilder<'a> {
    fn from_dev_info(dev_info: &'a DeviceInfo) -> Option<Self> {
        match (dev_info.vendor_id(), dev_info.product_id()) {
            (VENDOR_ID, PRODUCT_ID_APP) => Some(Self {
                dev_info,
                build_fn: Box::from(|api| {
                    Ok(DeviceCommunicator::App(dev_info.open_device(api)?.into()))
                }),
            }),
            (VENDOR_ID, PRODUCT_ID_BOOTLOADER) => Some(Self {
                dev_info,
                build_fn: Box::from(|api| {
                    Ok(DeviceCommunicator::Bootloader(
                        dev_info.open_device(api)?.into(),
                    ))
                }),
            }),
            _ => None,
        }
    }

    fn build(self, api: &'a HidApi) -> Result<DeviceCommunicator> {
        (self.build_fn)(api)
    }
}

impl<'a> Debug for CommunicatorBuilder<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("CommunicatorBuilder")
            .field("dev_info", &self.dev_info)
            .finish()
    }
}

impl<'a> PartialEq for CommunicatorBuilder<'a> {
    fn eq(&self, other: &Self) -> bool {
        self.dev_info.path() == other.dev_info.path()
    }
}

impl<'a> Eq for CommunicatorBuilder<'a> {}

impl<'a> Hash for CommunicatorBuilder<'a> {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.dev_info.path().hash(state);
    }
}

fn find_communicator(api: &HidApi) -> Result<Option<CommunicatorBuilder>> {
    let devices = HashSet::<_>::from_iter(
        api.device_list()
            .filter_map(CommunicatorBuilder::from_dev_info),
    );
    if devices.len() > 1 {
        // this is not supported since only one such physical device exists in the first place
        bail!("too many devices found: {:#?}", devices);
    }
    Ok(devices.into_iter().next())
}

#[allow(clippy::needless_return)] // https://github.com/rust-lang/rust-clippy/issues/12831
pub fn find_device() -> Result<Option<Device>> {
    let api = HidApi::new()?;
    return match find_communicator(&api)? {
        Some(comm_builder) => Ok(Some(Device {
            communicator: comm_builder.build(&api)?,
        })),
        None => Ok(None),
    };
}

const REFRESH_INTERVAL: Duration = Duration::from_millis(100);

fn wait_for_comm(timeout: Duration) -> Result<DeviceCommunicator> {
    let now = Instant::now();
    let mut api = HidApi::new()?;
    loop {
        if let Some(comm_builder) = find_communicator(&api)? {
            break comm_builder.build(&api);
        }
        if now.elapsed() > timeout {
            break Err(anyhow!(CommunicationError::DeviceReSearchTimeout));
        }
        thread::sleep(REFRESH_INTERVAL);
        api.refresh_devices()?;
    }
}
