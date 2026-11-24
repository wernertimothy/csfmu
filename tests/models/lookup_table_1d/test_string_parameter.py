"""Tests of the String parameter table_file of the lookup_table_1d model.

A String parameter is a char[MODEL_STRING_SIZE] field: it holds at most
MAX_STRING_BYTES bytes of UTF-8. fmi2SetString copies the text in and
refuses one that does not fit (never cutting it); fmi2GetString returns it.
table_file is fixed, so it can only be set until the initialization is finished.
"""

import pytest
from fmpy.fmi1 import FMICallException

from tests.fmu_helpers import MAX_STRING_BYTES, initialize


@pytest.fixture
def table_file(model_description):
    """The variable table_file of the modelDescription.xml."""
    return next(v for v in model_description.modelVariables if v.name == "table_file")


def get_text(fmu, vr) -> str:
    return fmu.getString([vr("table_file")])[0].decode("utf-8")


def set_text(fmu, vr, text) -> None:
    fmu.setString([vr("table_file")], [text])


def test_is_a_fixed_string_parameter(table_file) -> None:
    assert table_file.type == "String"
    assert table_file.causality == "parameter"
    assert table_file.variability == "fixed"
    assert table_file.initial == "exact"
    assert table_file.start == "table.csv"


def test_holds_its_start_value_after_instantiation(new_fmu, vr) -> None:
    assert get_text(new_fmu(), vr) == "table.csv"


def test_value_set_before_initialization_survives_it(new_fmu, vr) -> None:
    fmu = new_fmu()
    set_text(fmu, vr, "measured.csv")

    initialize(fmu)

    assert get_text(fmu, vr) == "measured.csv"


def test_can_be_set_in_initialization_mode(new_fmu, vr) -> None:
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    set_text(fmu, vr, "init.csv")
    fmu.exitInitializationMode()

    assert get_text(fmu, vr) == "init.csv"


def test_cannot_be_set_after_initialization(initialized_fmu, vr) -> None:
    """A fixed parameter can only be set before the simulation starts."""
    with pytest.raises(FMICallException):
        set_text(initialized_fmu, vr, "late.csv")
    assert get_text(initialized_fmu, vr) == "table.csv"


def test_longest_value_fits(new_fmu, vr) -> None:
    fmu = new_fmu()
    longest = "t" * (MAX_STRING_BYTES - 4) + ".csv"

    set_text(fmu, vr, longest)

    assert get_text(fmu, vr) == longest


def test_value_that_does_not_fit_is_refused_with_a_message(new_fmu, vr) -> None:
    log = []
    fmu = new_fmu(log=log)

    with pytest.raises(FMICallException):
        set_text(fmu, vr, "t" * (MAX_STRING_BYTES - 3) + ".csv")

    errors = [message for _, category, message in log if category == "logStatusError"]
    assert any(
        "'table_file' does not fit" in m and f"at most {MAX_STRING_BYTES} bytes" in m
        for m in errors
    ), errors
    assert get_text(fmu, vr) == "table.csv"  # not cut, the old value stays


def test_utf8_value_round_trips(new_fmu, vr) -> None:
    fmu = new_fmu()
    set_text(fmu, vr, "Tabelle_äöü.csv")
    assert get_text(fmu, vr) == "Tabelle_äöü.csv"


def test_limit_counts_bytes_not_characters(new_fmu, vr) -> None:
    """'ä' is two bytes of UTF-8: 254 + 2 bytes do not fit, although 255 characters would."""
    fmu = new_fmu()
    text = "a" * (MAX_STRING_BYTES - 1) + "ä"
    assert len(text) == MAX_STRING_BYTES
    assert len(text.encode("utf-8")) == MAX_STRING_BYTES + 1

    with pytest.raises(FMICallException):
        set_text(fmu, vr, text)


def test_null_value_is_refused(new_fmu, vr) -> None:
    """FMPy passes None as a NULL pointer."""
    fmu = new_fmu()
    with pytest.raises(FMICallException):
        set_text(fmu, vr, None)
    assert get_text(fmu, vr) == "table.csv"


def test_reset_restores_the_start_value(new_fmu, vr) -> None:
    fmu = new_fmu()
    set_text(fmu, vr, "other.csv")
    initialize(fmu)

    fmu.reset()

    assert get_text(fmu, vr) == "table.csv"


def test_fmu_state_restores_the_value(new_fmu, vr) -> None:
    fmu = new_fmu()
    set_text(fmu, vr, "saved.csv")
    state = fmu.getFMUstate()
    set_text(fmu, vr, "changed.csv")

    fmu.setFMUstate(state)

    assert get_text(fmu, vr) == "saved.csv"
    fmu.freeFMUstate(state)


def test_serialized_fmu_state_carries_the_value(new_fmu, vr) -> None:
    """The text is part of the serialized state, also in another instance."""
    fmu = new_fmu()
    set_text(fmu, vr, "saved.csv")
    state = fmu.getFMUstate()
    serialized = fmu.serializeFMUstate(state)
    fmu.freeFMUstate(state)

    other = new_fmu("other_instance")
    restored = other.deSerializeFMUstate(serialized)
    other.setFMUstate(restored)

    assert get_text(other, vr) == "saved.csv"
    other.freeFMUstate(restored)


def test_cannot_be_accessed_as_real(new_fmu, vr) -> None:
    with pytest.raises(FMICallException):
        new_fmu().getReal([vr("table_file")])
    with pytest.raises(FMICallException):
        new_fmu().setReal([vr("table_file")], [1.0])
