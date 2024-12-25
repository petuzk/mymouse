use anyhow::Result;
use std::{fs, path::PathBuf};

use crate::{
    avr_flashing::read_flash_data,
    cli::types::DeviceMode,
    communicator::{find_device, Device, DeviceCommunicator},
};

fn with_device(func: impl FnOnce(&mut Device) -> Result<()>) -> Result<()> {
    match find_device()? {
        None => {
            println!("Could not find the device.");
            Ok(())
        }
        Some(mut device) => func(&mut device),
    }
}

pub fn show_current_mode() -> Result<()> {
    with_device(|device| {
        println!(
            "Device is currently in {} mode.",
            match device.current_communicator() {
                DeviceCommunicator::App(_) => "app",
                DeviceCommunicator::Bootloader(_) => "bootloader",
            }
        );
        Ok(())
    })
}

pub fn change_mode(mode: DeviceMode) -> Result<()> {
    with_device(|device| {
        println!("Changing device mode to {}", mode);
        match mode {
            DeviceMode::App => drop(device.app_communicator()?),
            DeviceMode::Bootloader => drop(device.bootloader_communicator()?),
        };
        Ok(())
    })
}

pub fn flash(file_path: &PathBuf) -> Result<()> {
    let elf_content = fs::read(file_path)?;
    let flash_data = read_flash_data(&elf_content)?;
    println!("Read {flash_data} from {}", file_path.display());
    // Ok(())
    with_device(|device| device.bootloader_communicator()?.flash(&flash_data))
}
