use clap::ValueEnum;
use core::fmt;

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ValueEnum)]
pub enum DeviceMode {
    App,
    Bootloader,
}

impl fmt::Display for DeviceMode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DeviceMode::App => f.write_str("app"),
            DeviceMode::Bootloader => f.write_str("bootloader"),
        }
    }
}
