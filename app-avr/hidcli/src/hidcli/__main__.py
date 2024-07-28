import sys

from hidcli import find_device


def main():
    dev = find_device()
    if dev is None:
        print('Could not find the device.')
        return

    with dev:
        print(f'Device is in {dev.mode} mode.')

        if '-b' in sys.argv:
            if dev.mode == 'app':
                print('Entering bootloader')
                dev.mode = 'bootloader'
                print(f'Device is in {dev.mode} mode.')
            else:
                print('Device is already in bootloader, ignoring -b')

        if '-a' in sys.argv:
            if dev.mode == 'bootloader':
                print('Leaving bootloader')
                dev.mode = 'app'
                print(f'Device is in {dev.mode} mode.')
            else:
                print('Device is already in app, ignoring -a')


main()
