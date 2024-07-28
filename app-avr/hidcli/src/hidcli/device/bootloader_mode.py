import time

import hid

from .mode import DeviceMode, ModeCommunicator


class BootloaderCommunicator(ModeCommunicator):
    MODE = DeviceMode.BOOTLOADER

    def __init__(self, dev: hid.Device):
        self.dev = dev

    def set_mode(self, new_mode: DeviceMode):
        assert new_mode == DeviceMode.APP
        self.dev.write(b'\x00\xff\xff')  # zero report ID, address 0xFFFF
        time.sleep(1)  # give the device some time to reset itself and enumerate to USB
        return None  # the device re-enumerates to USB bus, and should be found again
