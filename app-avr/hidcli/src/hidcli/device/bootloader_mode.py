import struct
import time

import hid

from .mode import DeviceMode, ModeCommunicator

FLASH_PAGE_SIZE_BYTES = 128  # see SPM_PAGESIZE in iom8u2.h


class BootloaderCommunicator(ModeCommunicator):
    MODE = DeviceMode.BOOTLOADER

    def __init__(self, dev: hid.Device):
        self.dev = dev

    def set_mode(self, new_mode: DeviceMode):
        assert new_mode is DeviceMode.APP
        self.dev.write(b'\x00\xff\xff')  # zero report ID, address 0xFFFF
        time.sleep(1)  # give the device some time to reset itself and enumerate to USB
        return None  # the device re-enumerates to USB bus, and should be found again

    def flash(self, start_addr: int, program: bytes):
        """Flash the `program` at `start_addr` (offset in bytes).

        Note: this method does not switch device mode to `APP`.
        """
        if not _is_addr_aligned(start_addr):
            raise ValueError(f'page address is not aligned')
        if (unaligned_size := len(program) % FLASH_PAGE_SIZE_BYTES):
            program += bytes(FLASH_PAGE_SIZE_BYTES - unaligned_size)
        for byte_offset in range(0, len(program), FLASH_PAGE_SIZE_BYTES):
            page_addr = (start_addr + byte_offset) // 2
            self.dev.write(b''.join((
                b'\x00',
                struct.pack('<H', page_addr),  # little-endian short
                program[byte_offset:byte_offset + FLASH_PAGE_SIZE_BYTES]
            )))


def _is_addr_aligned(addr):
    return addr & (FLASH_PAGE_SIZE_BYTES - 1) == 0
