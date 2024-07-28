from intelhex import IntelHex


def load_program(source) -> 'tuple[int, bytes]':
    reader = IntelHex(source)
    start_addr = reader.minaddr()
    if reader.maxaddr() - start_addr + 1 != len(reader):
        # support for non-contiguous data is not needed yet
        # if needed, this function can be turned into generator yielding contiguous chunks
        raise ValueError('source contains non-contiguous data')
    return start_addr, reader.tobinstr()
