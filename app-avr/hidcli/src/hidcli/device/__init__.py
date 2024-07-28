import hid
from more_itertools import map_except

from .app_mode import ApplicationCommunicator
from .bootloader_mode import BootloaderCommunicator
from .mode import DeviceMode, ModeCommunicator

__all__ = (
    # re-exports
    'ApplicationCommunicator', 'BootloaderCommunicator', 'DeviceMode', 'ModeCommunicator',
    # things defined here
    'Device', 'MultipleDevicesFoundError', 'find_device', 'find_communicator',
)


class MultipleDevicesFoundError(RuntimeError):
    ...


def find_device() -> 'Device | None':
    """Get the currently connected device, or None if such device is not found.

    If multiple connected devices found, raise `MultipleDevicesFoundError`.
    """
    communicator = find_communicator()
    return Device(communicator) if communicator else None


def find_communicator() -> 'ModeCommunicator | None':
    """Get the ModeCommunicator for the currently connected device, or None if such device is not found.

    If multiple connected devices found, raise `MultipleDevicesFoundError`.
    Typically `find_device()` should be used instead of this function.
    """
    # on macOS, hid.enumerate() may return multiple entries for one device (with different usage_page/usage)
    # use set to only get one item per recognized device
    matching_devices = set(map_except(_get_comm_args_for_dev_info, hid.enumerate(), KeyError))

    if not matching_devices:
        return None
    if len(matching_devices) > 1:
        raise MultipleDevicesFoundError(f'more than one device found: {matching_devices}')

    cls, path = matching_devices.pop()
    return cls(hid.Device(path=path))


class Device:
    """Represents a physical device (the mouse).

    The physical device might be in different modes (see `DeviceMode`), which use different
    communication protocols and provide different functionality. This class is responsible for
    managing the connection and switching device mode. Actual communication is implemented by
    and delegated to mode-specific communicators (see `ModeCommunicator`).
    """
    def __init__(self, communicator: ModeCommunicator):
        self.communicator = communicator

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.communicator.dev.close()

    @property
    def mode(self) -> DeviceMode:
        return self.communicator.MODE

    @mode.setter
    def mode(self, new_mode: 'str | DeviceMode'):
        new_mode = DeviceMode(new_mode)
        # save previous mode for error message
        prev_mode = self.mode
        # don't do anything if the device is in requested mode
        if prev_mode == new_mode:
            return
        # otherwise replace communicator with the new one
        with self.communicator.dev:  # make sure the device is closed after set_mode()
            new_communicator = self.communicator.set_mode(new_mode)
        if new_communicator is None:
            new_communicator = find_communicator()
            if new_communicator is None:
                raise RuntimeError(f'could not find the device after switching mode from {prev_mode} to {new_mode}')
        self.communicator = new_communicator
        # make sure the new mode is as requested
        if self.mode != new_mode:
            raise RuntimeError(
                f'device is in {self.mode} mode after attempting to switch from {prev_mode} to {new_mode}')

    def bootloader(self) -> BootloaderCommunicator:
        self.mode = DeviceMode.BOOTLOADER
        return self.communicator

    def app(self) -> ApplicationCommunicator:
        self.mode = DeviceMode.APP
        return self.communicator


VENDOR_ID = 0x03EB
PRODUCT_ID_APP = 0x2041
PRODUCT_ID_BOOTLOADER = 0x2067


def _get_comm_args_for_dev_info(device_info: dict
                                ) -> 'tuple[type[ApplicationCommunicator] | type[BootloaderCommunicator], str]':
    """Return device class and device path for the given `device_info` dictionary describing recognized device.

    Raise KeyError if the device is not recognized.
    The `device_info` dictionary should come from hid.DeviceInfo structure.
    """
    cls = {
        (VENDOR_ID, PRODUCT_ID_APP): ApplicationCommunicator,
        (VENDOR_ID, PRODUCT_ID_BOOTLOADER): BootloaderCommunicator,
    }[device_info['vendor_id'], device_info['product_id']]
    return (cls, device_info['path'])
