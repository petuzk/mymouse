import hid

from more_itertools import map_except

DEV_VENDOR_ID = 0x03EB
DEV_PRODUCT_ID_APP = 0x2041
DEV_PRODUCT_ID_BOOTLOADER = 0x2067


def get_device() -> 'ApplicationDevice | BootloaderDevice | None':
    # on macOS, hid.enumerate() may return multiple entries for one device (with different usage_page/usage)
    # use set to only get one item per recognized device
    matching_devices = set(map_except(_get_dev_args_for_dev_info, hid.enumerate(), KeyError))

    if not matching_devices:
        return None
    if len(matching_devices) > 1:
        raise ValueError(f'more than one device found: {matching_devices}')

    cls, path = matching_devices.pop()
    return cls(path)


def _get_dev_args_for_dev_info(device_info: dict) -> 'tuple[type[ApplicationDevice] | type[BootloaderDevice], str]':
    """Return device class and device path for the given `device_info` dictionary describing recognized device.

    Raise KeyError if the device is not recognized.
    The `device_info` dictionary should come from hid.DeviceInfo structure.
    """
    cls = {
        (DEV_VENDOR_ID, DEV_PRODUCT_ID_APP): ApplicationDevice,
        (DEV_VENDOR_ID, DEV_PRODUCT_ID_BOOTLOADER): BootloaderDevice,
    }[device_info['vendor_id'], device_info['product_id']]
    return (cls, device_info['path'])


class ApplicationDevice(hid.Device):
    @property
    def mode(self):
        return 'app'

    def __init__(self, path):
        super().__init__(path=path)

    def enter_bootloader(self):
        self.write(b'\x01\x42')  # first report ID, enter bootloader


class BootloaderDevice(hid.Device):
    @property
    def mode(self):
        return 'bootloader'

    def __init__(self, path):
        super().__init__(path=path)

    def leave_bootloader(self):
        self.write(b'\x00\xff\xff')  # zero report ID, address 0xFFFF
