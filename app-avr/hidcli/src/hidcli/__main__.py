import sys
import time

from hidcli import get_device


def main():
    dev = get_device()
    if dev is None:
        print('Could not find the device.')
        return

    with dev:
        print(f'Device is in {dev.mode} mode.')

        if '-b' in sys.argv:
            if dev.mode == 'app':
                print('Entering bootloader')
                dev.enter_bootloader()
                time.sleep(1)
            else:
                print('Device is already in bootloader, ignoring -b')

        if '-a' in sys.argv:
            if dev.mode == 'bootloader':
                print('Leaving bootloader')
                dev.leave_bootloader()
                time.sleep(1)
            else:
                print('Device is already in app, ignoring -a')

    if '-b' in sys.argv and dev.mode == 'app':
        dev = get_device()
        if dev is None:
            print('Could not find the device after restart.')
        else:
            print(f'Device is in {dev.mode} mode.')
            dev.close()

    if '-a' in sys.argv and dev.mode == 'bootloader':
        dev = get_device()
        if dev is None:
            print('Could not find the device after restart.')
        else:
            print(f'Device is in {dev.mode} mode.')
            dev.close()


main()
