import hid

from .mode import DeviceMode, ModeCommunicator


class ApplicationCommunicator(ModeCommunicator):
    MODE = DeviceMode.APP

    def __init__(self, dev: hid.Device):
        self.dev = dev

    def set_mode(self, new_mode: DeviceMode):
        assert new_mode is DeviceMode.BOOTLOADER
        self.dev.write(b'\x08\x42')  # report ID 8, enter bootloader command
        return None  # the device re-enumerates to USB bus, and should be found again
