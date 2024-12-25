use anyhow::Result;
use std::path::PathBuf;

use hidcli::cli::types::DeviceMode;

fn main() {
    if let Err(e) = actual_main() {
        eprintln!("Error: {:?}", e);
        std::process::exit(1);
    }
}

fn actual_main() -> Result<()> {
    let matches = dbg!(hidcli::cli::parsing::get_command().get_matches());

    match matches.subcommand() {
        None => hidcli::cli::actions::show_current_mode(),
        Some(("mode", sub_matches)) => hidcli::cli::actions::change_mode(
            *sub_matches
                .get_one::<DeviceMode>("MODE")
                .expect("required in clap"),
        ),
        Some(("flash", sub_matches)) => hidcli::cli::actions::flash(
            sub_matches
                .get_one::<PathBuf>("PATH")
                .expect("required in clap"),
        ),
        _ => unimplemented!(),
    }
}
