"""Model-independent tests of csfmu simulate on the FMU under test.

The recorded trajectory is what a change of the numerics is compared
against, so it has to be reproducible and has to be what the FMU computes.
"""

import pytest
from csfmu.simulate import format_value, simulate, write_csv

from tests.fmu_helpers import (
    changed_value,
    initialize,
    is_nan,
    read,
    start_value,
    variables_with,
)

# Number of communication steps of each run (with the FMU's default step size)
STEPS = 20


def short_run(path_to_fmu, model_description, **kwargs):
    """Simulate STEPS communication steps of the FMU's default step size from its default start time."""
    experiment = model_description.defaultExperiment
    start = float(experiment.startTime)
    step = float(experiment.stepSize)
    return simulate(
        path_to_fmu, start=start, stop=start + STEPS * step, step=step, **kwargs
    )


def same(recorded, expected) -> bool:
    """Whether a recorded value is the expected one (NaN counts as equal to NaN)."""
    return recorded == expected or (is_nan(recorded) and is_nan(expected))


def test_simulation_is_reproducible(path_to_fmu, model_description, tmp_path) -> None:
    """Two runs of the same FMU give byte-identical result files."""
    first, second = tmp_path / "first.csv", tmp_path / "second.csv"

    write_csv(first, short_run(path_to_fmu, model_description))
    write_csv(second, short_run(path_to_fmu, model_description))

    assert first.read_bytes() == second.read_bytes()


def test_every_communication_point_is_recorded(path_to_fmu, model_description) -> None:
    experiment = model_description.defaultExperiment
    start, step = float(experiment.startTime), float(experiment.stepSize)

    result = short_run(path_to_fmu, model_description)

    assert result.error is None
    assert result.times == [start + k * step for k in range(STEPS + 1)]
    assert len(result.rows) == STEPS + 1


def test_first_row_is_the_initialized_fmu(
    path_to_fmu, model_description, new_fmu
) -> None:
    """The row of the start time holds the values the FMU has after its initialization."""
    result = short_run(path_to_fmu, model_description)
    start = float(model_description.defaultExperiment.startTime)
    fmu = initialize(new_fmu(), startTime=start)

    for variable, recorded in zip(result.variables, result.rows[0]):
        expected = read(fmu, variable)
        if variable.type == "Boolean":
            expected = bool(expected)
        assert same(recorded, expected), (
            f"'{variable.name}' was recorded as {recorded!r} at t={start}, "
            f"but the initialized FMU has {expected!r}"
        )


def test_input_file_is_held_from_its_row_on(
    path_to_fmu, model_description, tmp_path
) -> None:
    """An input row between two communication points holds from the next one on."""
    inputs = variables_with(model_description, "input")
    if not inputs:
        pytest.skip("the model has no inputs")
    variable = inputs[0]
    experiment = model_description.defaultExperiment
    start, step = float(experiment.startTime), float(experiment.stepSize)
    value = changed_value(variable)
    input_file = tmp_path / "input.csv"
    input_file.write_text(
        f"time,{variable.name}\n{start + 2.5 * step!r},{format_value(variable, value)}\n",
        encoding="utf-8",
    )

    result = short_run(path_to_fmu, model_description, input_file=input_file)

    assert result.error is None
    # simulate() reads its own modelDescription, so find the column by name
    index = [v.name for v in result.variables].index(variable.name)
    column = [row[index] for row in result.rows]
    assert column == [start_value(variable)] * 3 + [value] * (STEPS - 2)
