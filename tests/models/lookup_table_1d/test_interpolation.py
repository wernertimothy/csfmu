"""Tests of the lookup_table_1d model: y = interp1(table, u), without states.

The packaged table (resources/table.csv) holds the points (u, y) = (-1, -2),
(0, 0), (1, 1), (2, 1.5), (3, 1.75). Between two points y is interpolated
linearly, outside the table it is clamped to the first or last value. All
values below are exact in binary floating point. The initialization reads
the table, so y is NaN before.
"""

import pytest

from tests.fmu_helpers import advance, initialize, is_nan, states

# (u, y): the points, the middle of every segment, and clamping on both sides
EXPECTED = [
    (-2.0, -2.0),
    (-1.0, -2.0),
    (-0.5, -1.0),
    (0.0, 0.0),
    (0.5, 0.5),
    (1.0, 1.0),
    (1.5, 1.25),
    (2.0, 1.5),
    (2.5, 1.625),
    (3.0, 1.75),
    (4.0, 1.75),
]


def test_model_has_no_states(model_description) -> None:
    assert states(model_description) == []


@pytest.mark.parametrize(("u", "y"), EXPECTED)
def test_output_follows_the_input_without_a_step(initialized_fmu, vr, u, y) -> None:
    """Direct feedthrough: y is the table value right after u is set."""
    initialized_fmu.setReal([vr("u")], [u])
    assert initialized_fmu.getReal([vr("y")])[0] == y


def test_output_is_nan_before_initialization(new_fmu, vr) -> None:
    """The initialization reads the table: before, y cannot be computed."""
    fmu = new_fmu()
    assert is_nan(fmu.getReal([vr("y")])[0])

    fmu.setReal([vr("u")], [0.5])
    assert is_nan(fmu.getReal([vr("y")])[0])

    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    assert is_nan(fmu.getReal([vr("y")])[0])

    fmu.exitInitializationMode()
    assert fmu.getReal([vr("y")])[0] == 0.5


def test_output_is_nan_again_after_reset(initialized_fmu, vr) -> None:
    """fmi2Reset forgets the table until the next initialization."""
    initialized_fmu.setReal([vr("u")], [0.5])
    assert initialized_fmu.getReal([vr("y")])[0] == 0.5

    initialized_fmu.reset()
    assert is_nan(initialized_fmu.getReal([vr("y")])[0])

    initialize(initialized_fmu)
    assert (
        initialized_fmu.getReal([vr("y")])[0] == 0.0
    )  # u is back at its start value 0


def test_output_after_steps(initialized_fmu, vr) -> None:
    """Without states a step only advances the time; y keeps the table value."""
    initialized_fmu.setReal([vr("u")], [2.5])

    end = advance(initialized_fmu, 1.0, step_size=0.1)

    assert end == pytest.approx(1.0)
    assert initialized_fmu.getReal([vr("y")])[0] == 1.625


def test_output_follows_a_new_input_between_steps(new_fmu, vr) -> None:
    fmu = initialize(new_fmu())
    fmu.setReal([vr("u")], [-0.5])
    advance(fmu, 0.5, step_size=0.1)

    fmu.setReal([vr("u")], [1.5])

    assert fmu.getReal([vr("y")])[0] == 1.25
    advance(fmu, 0.5, step_size=0.1, start=0.5)
    assert fmu.getReal([vr("y")])[0] == 1.25
