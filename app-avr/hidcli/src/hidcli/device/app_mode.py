import hid

from .mode import DeviceMode, ModeCommunicator


class ApplicationCommunicator(ModeCommunicator):
    MODE = DeviceMode.APP

    def __init__(self, dev: hid.Device):
        self.dev = dev

    def set_mode(self, new_mode: DeviceMode):
        assert new_mode is DeviceMode.BOOTLOADER
        self.dev.write(b'\x01\x42')  # first report ID, enter bootloader command
        return None  # the device re-enumerates to USB bus, and should be found again
