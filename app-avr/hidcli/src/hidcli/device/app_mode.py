import time

import hid

from .mode import DeviceMode, ModeCommunicator


class ApplicationCommunicator(ModeCommunicator):
    MODE = DeviceMode.APP

    def __init__(self, dev: hid.Device):
        self.dev = dev

    def set_mode(self, new_mode: DeviceMode):
        assert new_mode == DeviceMode.BOOTLOADER
        self.dev.write(b'\x01\x42')  # first report ID, enter bootloader command
        time.sleep(1)  # give the device some time to reset itself and enumerate to USB
        return None  # the device re-enumerates to USB bus, and should be found again
