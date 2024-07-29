from typing import Protocol

import hid

try:
    from enum import StrEnum
except ImportError:
    from enum import Enum

    class StrEnum(Enum):
        def __eq__(self, other):
            return self.value == other or super().__eq__(other)

        def __str__(self) -> str:
            return self.value


class DeviceMode(StrEnum):
    APP = 'app'
    BOOTLOADER = 'bootloader'


class ModeCommunicator(Protocol):
    """A class implementing communication with the device in the specified mode.

    The `MODE` is not a state of a `Communicator`, but a state of a device it communicates with,
    and thus is expected to be constant between `set_mode` calls. The device object `dev`
    is passed to the communicator opened, and is closed after the communicator is no longer needed.
    """
    MODE: DeviceMode
    dev: hid.Device

    def set_mode(self, new_mode: DeviceMode) -> 'ModeCommunicator | None':
        """Request the device to change mode to `new_mode`.

        Return a new communicator corresponding to that mode, or None if the device should be found.
        This function can assume `new_mode` is always different from `self.MODE`.
        """
