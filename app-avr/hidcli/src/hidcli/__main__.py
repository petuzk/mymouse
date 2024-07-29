from argparse import ArgumentParser, FileType

from hidcli import find_device
from hidcli.device import DeviceMode
from hidcli.intelhex_reader import load_program


def main():
    parser = ArgumentParser()
    subparsers = parser.add_subparsers(help='Subcommands')

    flash_parser = subparsers.add_parser('flash', help='Flash an application image via HID bootloader')
    flash_parser.add_argument('image', type=FileType(), help='Path to .hex file (or "-" to read from stdin)')

    mode_parser = subparsers.add_parser('mode', help='Change device mode')
    mode_parser.add_argument('mode', choices=list(DeviceMode), help='Desired mode')

    args = parser.parse_args()

    dev = find_device()
    if dev is None:
        print('Could not find the device.')
        return

    with dev:
        print(f'Device is in {dev.mode} mode.')

        if hasattr(args, 'mode'):
            if dev.mode == args.mode:
                print(f'Skipping changing device mode to {args.mode}')
            else:
                print('Entering bootloader' if args.mode == DeviceMode.BOOTLOADER else 'Leaving bootloader')
                dev.mode = args.mode
                print(f'Device is in {dev.mode} mode.')
            return

        if hasattr(args, 'image'):
            print(f'Flashing device from {args.image.name}')
            dev.bootloader().flash(*load_program(args.image))
            print(f'Flashing completed, switching to app')
            dev.mode = DeviceMode.APP
            print(f'Device is in {dev.mode} mode.')
            return


main()
