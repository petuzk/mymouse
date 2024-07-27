import hid

DEV_VENDOR_ID = 0x03EB
DEV_PRODUCT_ID_APP = 0x2041
DEV_PRODUCT_ID_BOOTLOADER = 0x2067


def get_device() -> 'ApplicationDevice | BootloaderDevice | None':
    device_product_ids = {DEV_PRODUCT_ID_APP, DEV_PRODUCT_ID_BOOTLOADER}
    matching_devices = [
        device for device in hid.enumerate()
        if device['vendor_id'] == DEV_VENDOR_ID and device['product_id'] in device_product_ids
    ]

    if not matching_devices:
        return None
    if len(matching_devices) > 1:
        raise ValueError(f'more than one device found: {matching_devices}')

    device = matching_devices[0]
    cls = ApplicationDevice if device['product_id'] == DEV_PRODUCT_ID_APP else BootloaderDevice
    return cls(device['path'])


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
