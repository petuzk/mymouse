from hidcli.device import BootloaderCommunicator


def test_flash(fakedev):
    dev = BootloaderCommunicator(fakedev)
    dev.flash(0x1800, bytes(range(128)))

    assert fakedev.writes == [
        b'\x00\x00\x0C' + bytes(range(0, 64)),
        b'\x00\x20\x0C' + bytes(range(64, 128)),
    ]
