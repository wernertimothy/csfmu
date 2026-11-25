"""Tests of reading the table file of the lookup_table_1d model.

The initialization reads the table from resources/<table_file> of the FMU:
a header line, then one point "u,y" per line with u strictly increasing,
'.' as decimal point, at most 256 points. The tests write their own table
files into the resources of the extracted FMU (fixture write_table), whose
path contains a space and a non-ASCII character (see tests/conftest.py).
"""

import locale

import pytest
from fmpy.fmi1 import FMICallException
from fmpy.fmi2 import fmi2Error

from tests.fmu_helpers import initialize


def start_with(fmu, vr, table_file: str):
    """Set table_file and initialize the FMU; returns the FMU."""
    fmu.setString([vr("table_file")], [table_file])
    return initialize(fmu)


def y_at(fmu, vr, u: float) -> float:
    fmu.setReal([vr("u")], [u])
    return fmu.getReal([vr("y")])[0]


def initialization_error(new_fmu, vr, table_file: str) -> str:
    """Initialize with table_file, expect fmi2ExitInitializationMode to fail
    and return the error messages of the log."""
    log = []
    fmu = new_fmu(log=log)
    fmu.setString([vr("table_file")], [table_file])
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    with pytest.raises(FMICallException):
        fmu.exitInitializationMode()

    errors = [message for _, category, message in log if category == "logStatusError"]
    assert errors, log
    return "\n".join(errors)


def test_packaged_table_is_a_resource(resources) -> None:
    assert (resources / "table.csv").is_file()


def test_another_table_changes_the_output(new_fmu, vr, write_table) -> None:
    """The model really reads the file: a steeper table gives other values."""
    fmu = start_with(new_fmu(), vr, write_table("steep.csv", "u,y\n0,0\n1,10\n"))

    assert y_at(fmu, vr, 0.5) == 5.0
    assert y_at(fmu, vr, 2.0) == 10.0


def test_missing_file_fails_the_initialization_with_its_path(
    new_fmu, vr, resources
) -> None:
    error = initialization_error(new_fmu, vr, "missing.csv")

    path = (resources / "missing.csv").as_posix()
    assert f"Cannot open the table file '{path}'" in error


def test_empty_table_file_fails_the_initialization(new_fmu, vr) -> None:
    assert "table_file is empty" in initialization_error(new_fmu, vr, "")


@pytest.mark.parametrize(
    ("text", "message"),
    [
        ("u,y\n0,0\n1,1\n1,2\n", "Line 4 of the table file"),
        ("u,y\n0,0\n2,1\n1,2\n", "Line 4 of the table file"),
    ],
    ids=["repeated_u", "decreasing_u"],
)
def test_u_must_increase_strictly(new_fmu, vr, write_table, text, message) -> None:
    error = initialization_error(new_fmu, vr, write_table("not_increasing.csv", text))
    assert message in error
    assert "strictly increasing" in error


@pytest.mark.parametrize(
    "line",
    [
        "1;1",
        "1,1,1",
        "1,abc",
        "abc,1",
        "1,",
        ",1",
        "1 2",
        "1,1x",
        "0x1,1",
        "inf,1",
        "nan,1",
        "1,2,",
    ],
)
def test_malformed_line_fails_the_initialization(
    new_fmu, vr, write_table, line
) -> None:
    error = initialization_error(
        new_fmu, vr, write_table("malformed.csv", f"u,y\n0,0\n{line}\n")
    )
    assert "Line 3 of the table file" in error
    assert "is not a point" in error


def test_value_that_is_not_finite_fails_the_initialization(
    new_fmu, vr, write_table
) -> None:
    error = initialization_error(
        new_fmu, vr, write_table("huge.csv", "u,y\n0,0\n1,1e999\n")
    )
    assert "Line 3 of the table file" in error
    assert "not finite" in error


def test_first_line_must_be_a_header(new_fmu, vr, write_table) -> None:
    error = initialization_error(
        new_fmu, vr, write_table("no_header.csv", "0,0\n1,1\n2,2\n")
    )
    assert "must be a header" in error


@pytest.mark.parametrize(("text", "points"), [("u,y\n", 0), ("u,y\n0,0\n", 1), ("", 0)])
def test_table_needs_two_points(new_fmu, vr, write_table, text, points) -> None:
    error = initialization_error(new_fmu, vr, write_table("short.csv", text))
    assert f"has {points} point(s), at least 2 are needed" in error


def test_table_may_have_256_points(new_fmu, vr, write_table) -> None:
    rows = "".join(f"{k},{2 * k}\n" for k in range(256))
    fmu = start_with(new_fmu(), vr, write_table("long.csv", "u,y\n" + rows))

    assert y_at(fmu, vr, 254.5) == 509.0
    assert y_at(fmu, vr, 1000.0) == 510.0


def test_table_must_not_have_257_points(new_fmu, vr, write_table) -> None:
    rows = "".join(f"{k},{2 * k}\n" for k in range(257))
    error = initialization_error(
        new_fmu, vr, write_table("too_long.csv", "u,y\n" + rows)
    )
    assert "has more than 256 points" in error


def test_line_that_is_too_long_fails_the_initialization(
    new_fmu, vr, write_table
) -> None:
    long_line = "0," + "0" * 300
    error = initialization_error(
        new_fmu, vr, write_table("long_line.csv", f"u,y\n{long_line}\n1,1\n")
    )
    assert "Line 2 of the table file" in error
    assert "is longer than" in error


def test_windows_line_breaks_blank_lines_and_blanks_are_accepted(
    new_fmu, vr, write_table
) -> None:
    text = "u,y\n\n 0 , 0 \n\t1.0,\t1e1\n\n2.5E0 , -2.5e-1\n\n"
    fmu = start_with(new_fmu(), vr, write_table("relaxed.csv", text, newline="\r\n"))

    assert y_at(fmu, vr, 0.5) == 5.0
    assert y_at(fmu, vr, 2.5) == -0.25


def test_last_line_without_line_break_is_read(new_fmu, vr, write_table) -> None:
    fmu = start_with(new_fmu(), vr, write_table("no_final_break.csv", "u,y\n0,0\n1,4"))
    assert y_at(fmu, vr, 0.25) == 1.0


@pytest.mark.parametrize(
    "name", ["my table.csv", "tabelle_ä.csv"], ids=["space", "non_ascii"]
)
def test_file_names_with_space_or_non_ascii_characters(
    new_fmu, vr, write_table, name
) -> None:
    fmu = start_with(new_fmu(), vr, write_table(name, "u,y\n0,0\n1,3\n"))
    assert y_at(fmu, vr, 0.5) == 1.5


COMMA_LOCALES = ["de_DE.UTF-8", "de_DE.utf8", "de_DE", "German_Germany.1252", "German"]


@pytest.fixture
def comma_locale():
    """Switch the C library to a locale with ',' as decimal point (as some
    integration tools do), and back afterwards. Skips if none is installed."""
    previous = locale.setlocale(locale.LC_NUMERIC)
    for name in COMMA_LOCALES:
        try:
            locale.setlocale(locale.LC_NUMERIC, name)
        except locale.Error:
            continue
        if locale.localeconv()["decimal_point"] == ",":
            break
    else:
        locale.setlocale(locale.LC_NUMERIC, previous)
        pytest.skip("no locale with ',' as decimal point is installed")
    yield
    locale.setlocale(locale.LC_NUMERIC, previous)


def test_table_reads_the_same_in_a_comma_locale(
    new_fmu, vr, write_table, comma_locale
) -> None:
    """The table file always uses '.', independent of the locale of the host process."""
    fmu = start_with(
        new_fmu(), vr, write_table("decimal.csv", "u,y\n0,0\n0.5,0.25\n1,1\n")
    )

    assert y_at(fmu, vr, 0.5) == 0.25
    assert y_at(fmu, vr, 0.75) == 0.625


# The table is cached: in initialization mode every read after a set
# initializes the model, but the file is only read again for another name.


def enter_initialization_mode(fmu, vr, table_file: str):
    """Set table_file and enter initialization mode; returns the FMU."""
    fmu.setString([vr("table_file")], [table_file])
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    return fmu


def test_initialization_mode_reads_the_table_file_once(
    new_fmu, vr, write_table, resources
) -> None:
    """After the first read the file is not needed any more: the table is cached."""
    name = write_table("cached.csv", "u,y\n0,0\n1,10\n")
    fmu = enter_initialization_mode(new_fmu(), vr, name)
    assert y_at(fmu, vr, 0.5) == 5.0

    (resources / name).unlink()
    assert y_at(fmu, vr, 0.25) == 2.5

    fmu.exitInitializationMode()
    assert y_at(fmu, vr, 0.75) == 7.5


def test_another_table_file_is_read_in_initialization_mode(
    new_fmu, vr, write_table
) -> None:
    """The cache belongs to one file: setting another table_file reads that one."""
    first = write_table("first.csv", "u,y\n0,0\n1,10\n")
    second = write_table("second.csv", "u,y\n0,0\n1,100\n")
    fmu = enter_initialization_mode(new_fmu(), vr, first)
    assert y_at(fmu, vr, 0.5) == 5.0

    fmu.setString([vr("table_file")], [second])
    assert y_at(fmu, vr, 0.5) == 50.0


def test_missing_table_file_fails_the_read_in_initialization_mode(new_fmu, vr) -> None:
    """A read initializes the model, so it fails like fmi2ExitInitializationMode would."""
    log = []
    fmu = enter_initialization_mode(new_fmu(log=log), vr, "table.csv")
    assert y_at(fmu, vr, 0.5) == 0.5

    fmu.setString([vr("table_file")], ["missing.csv"])
    with pytest.raises(FMICallException) as failure:
        fmu.getReal([vr("y")])

    assert failure.value.status == fmi2Error
    assert any(
        "Initialization failed" in message and "missing.csv" in message
        for _, _, message in log
    ), log


def test_reset_forgets_the_cached_table(new_fmu, vr, write_table, resources) -> None:
    """fmi2Reset zeroes the internals, so the next initialization reads the file again."""
    name = write_table("forgotten.csv", "u,y\n0,0\n1,10\n")
    fmu = start_with(new_fmu(), vr, name)
    assert y_at(fmu, vr, 0.5) == 5.0

    (resources / name).unlink()
    fmu.reset()

    with pytest.raises(FMICallException):
        start_with(fmu, vr, name)
