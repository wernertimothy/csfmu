"""Fixtures of the tests of the lookup_table_1d model.

The model reads its table from resources/<table_file> of the FMU. The tests
write their own table files into the resources of the extracted FMU.
"""

import pytest

# The points of the packaged table (src/models/lookup_table_1d/resources/table.csv)
PACKAGED_TABLE = "u,y\n-1,-2\n0,0\n1,1\n2,1.5\n3,1.75\n"


@pytest.fixture
def resources(unzipdir):
    """The resources folder of the extracted FMU."""
    return unzipdir / "resources"


@pytest.fixture
def write_table(resources):
    """Return a function that writes a table file into the resources of the
    extracted FMU and returns its name (the value for table_file).

    The FMU is extracted once per session, so the files a test writes are
    removed after the test.
    """
    written = []

    def write(name: str, text: str = PACKAGED_TABLE, newline: str = "\n") -> str:
        path = resources / name
        assert name != "table.csv", "the packaged table must not be overwritten"
        path.write_bytes(text.replace("\n", newline).encode("utf-8"))
        written.append(path)
        return name

    yield write

    for path in written:
        path.unlink(missing_ok=True)
