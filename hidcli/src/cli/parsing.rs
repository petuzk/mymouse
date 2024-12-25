use clap::{arg, value_parser, Command};
use std::path::PathBuf;

use crate::cli::types::DeviceMode;

pub fn get_command() -> Command {
    Command::new("hidcli")
        .about("CLI tools for my mouse")
        .subcommand(
            Command::new("mode")
                .about("Change mouse mode")
                .arg(arg!(<MODE> "Desired mode").value_parser(value_parser!(DeviceMode)))
                .arg_required_else_help(true),
        )
        .subcommand(
            Command::new("flash")
                .about("Flash application into AVR via bootloader")
                .arg_required_else_help(true)
                .arg(arg!(<PATH> "Image (.hex file)").value_parser(clap::value_parser!(PathBuf))),
        )
}
