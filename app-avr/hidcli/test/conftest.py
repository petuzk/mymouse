import pytest


@pytest.fixture
def fakedev():
    return FakeDevice()


class FakeDevice:
    def __init__(self):
        self.writes = []
        self.closed = False

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.close()

    def close(self):
        self.closed = True

    def write(self, data):
        if self.closed:
            pytest.fail('writing into closed device')
        self.writes.append(data)
