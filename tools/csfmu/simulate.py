"""Simulate an FMU with a fixed-step master and record its trajectory.

The master is deliberately simple and deterministic, so that two builds of a
model can be compared bit for bit:

- The communication points are t_k = start + k*step with an integer k, never
  accumulated.
- Inputs come from a CSV file and are held constant (zero-order hold): at
  t_k an input takes the value of the last row whose time is not after t_k.
- At every communication point the inputs of t_k are set, then the
  variables are recorded, then the FMU steps to t_k+1.
- Real values are written with the shortest text that reads back to the same
  double (repr), so two bit-identical trajectories give byte-identical files.
"""

import bisect
import csv
import math
import tempfile
from dataclasses import dataclass
from pathlib import Path

import fmpy
import fmpy.fmi2
from fmpy.fmi1 import FMICallException

# Getters and setters of FMPy's FMU2Slave per variable type. Enumeration
# variables are accessed like Integer variables (FMI 2.0)
GETTERS = {
    "Real": "getReal",
    "Integer": "getInteger",
    "Enumeration": "getInteger",
    "Boolean": "getBoolean",
}
SETTERS = {
    "Real": "setReal",
    "Integer": "setInteger",
    "Enumeration": "setInteger",
    "Boolean": "setBoolean",
    "String": "setString",
}

# Log categories of the FMU that are printed during a simulation: warnings
# and errors, each with the FMU's own message (see LOG_CATEGORIES in
# src/fmi2Functions.c)
LOG_CATEGORIES = ["logStatusWarning", "logStatusError"]

# Relative tolerance, in units of the step size, for times that should be
# equal but may differ by rounding: the stop time on the grid, and the time
# of an input row that falls on a communication point
GRID_TOLERANCE = 1e-9

BOOLEAN_TEXTS = {"true": True, "1": True, "false": False, "0": False}


class SimulationError(Exception):
    """The simulation cannot be set up (bad arguments, bad input file, bad FMU)."""


@dataclass
class Result:
    """The recorded trajectory: one row of values per communication point.

    The rows are complete up to the last communication point the FMU
    reached. If the simulation stopped early, error says why.
    """

    variables: list
    times: list[float]
    rows: list[list]
    error: str | None = None

    @property
    def columns(self) -> list[str]:
        return ["time", *(variable.name for variable in self.variables)]


@dataclass
class InputTable:
    """Input values over time, read from a CSV file (see read_inputs())."""

    variables: list
    times: list[float]
    rows: list[list]

    def active_row(self, time: float, tolerance: float) -> int | None:
        """Index of the row that holds at time: the last row whose time is not after time.

        A row up to tolerance after time counts as at time, so that a row on a
        communication point applies there despite rounding. With several rows
        at the same time the last one wins. None before the first row.
        """
        row = bisect.bisect_right(self.times, time + tolerance) - 1
        return row if row >= 0 else None

    def values(self, row: int) -> list[tuple]:
        """The (variable, value) pairs of a row."""
        return list(zip(self.variables, self.rows[row]))


def number_of_steps(start: float, stop: float, step: float) -> int:
    """Number of communication steps from start to stop; stop must be on the grid start + k*step."""
    for name, value in (("start", start), ("stop", stop), ("step", step)):
        if not math.isfinite(value):
            raise SimulationError(
                f"the {name} time must be a finite number, not {value!r}"
            )
    if step <= 0.0:
        raise SimulationError(f"the step size must be greater than 0, not {step!r}")
    if stop <= start:
        raise SimulationError(
            f"the stop time {stop!r} must be after the start time {start!r}"
        )
    steps = round((stop - start) / step)
    if steps < 1 or abs(start + steps * step - stop) > GRID_TOLERANCE * step:
        raise SimulationError(
            f"the stop time {stop!r} is not a whole number of steps of {step!r} after the start time {start!r}"
        )
    return steps


def parse_value(variable, text: str):
    """Convert a text to a value of the variable's type."""
    text = text.strip()
    try:
        if variable.type == "Real":
            return float(text)
        if variable.type in ("Integer", "Enumeration"):
            return int(text)
        if variable.type == "Boolean":
            return BOOLEAN_TEXTS[text.lower()]
    except KeyError, ValueError:
        expected = (
            "true, false, 1 or 0"
            if variable.type == "Boolean"
            else f"a {variable.type} value"
        )
        raise SimulationError(
            f"'{text}' is not a value of '{variable.name}' (expected {expected})"
        ) from None
    return text


def format_value(variable, value) -> str:
    """The text of a recorded value: Real values exactly (shortest round trip), Boolean values as 1 or 0."""
    if variable.type == "Real":
        return repr(float(value))
    if variable.type == "Boolean":
        return "1" if value else "0"
    return str(int(value))


def recorded_variables(model_description) -> list:
    """The variables that are recorded: inputs, outputs and local variables (states and locals).

    Parameters are left out because they are constant during a simulation,
    String variables because they are parameters only.
    """
    return [
        variable
        for variable in model_description.modelVariables
        if variable.causality in ("input", "output", "local")
        and variable.type != "String"
    ]


def read_inputs(path: Path, model_description) -> InputTable:
    """Read an input file: a CSV file with a header, the column time first, then one column per input.

    The times must not decrease. Every column after time must be an input
    variable of the model.
    """
    inputs = {
        variable.name: variable
        for variable in model_description.modelVariables
        if variable.causality == "input"
    }
    try:
        with open(path, newline="", encoding="utf-8") as file:
            lines = list(csv.reader(file))
    except OSError as error:
        raise SimulationError(
            f"cannot read the input file {path}: {error.strerror}"
        ) from None

    lines = [
        (number, line)
        for number, line in enumerate(lines, start=1)
        if any(cell.strip() for cell in line)
    ]
    if not lines:
        raise SimulationError(f"the input file {path} is empty")
    header = [name.strip() for name in lines[0][1]]
    if header[0] != "time":
        raise SimulationError(
            f"the first column of the input file {path} must be 'time', not '{header[0]}'"
        )

    variables = []
    for name in header[1:]:
        if name not in inputs:
            known = ", ".join(inputs) if inputs else "the model has none"
            raise SimulationError(
                f"column '{name}' of the input file is not an input of the model (inputs: {known})"
            )
        if inputs[name] in variables:
            raise SimulationError(f"the input file has two columns '{name}'")
        variables.append(inputs[name])

    times = []
    rows = []
    for number, line in lines[1:]:
        if len(line) != len(header):
            raise SimulationError(
                f"line {number} of the input file has {len(line)} values, the header {len(header)}"
            )
        try:
            time = float(line[0])
        except ValueError:
            time = math.nan
        if not math.isfinite(time):
            raise SimulationError(
                f"line {number} of the input file: '{line[0].strip()}' is not a time"
            )
        if times and time < times[-1]:
            raise SimulationError(
                f"line {number} of the input file: the time {time!r} is before {times[-1]!r}"
            )
        try:
            values = [
                parse_value(variable, text)
                for variable, text in zip(variables, line[1:])
            ]
        except SimulationError as error:
            raise SimulationError(f"line {number} of the input file: {error}") from None
        times.append(time)
        rows.append(values)
    return InputTable(variables, times, rows)


def set_values(fmu, pairs) -> None:
    """Set (variable, value) pairs with the setter that matches each variable's type."""
    for variable, value in pairs:
        getattr(fmu, SETTERS[variable.type])([variable.valueReference], [value])


def run(
    fmu,
    recorded: list,
    start: float,
    step: float,
    steps: int,
    inputs: InputTable | None = None,
    start_values: list[tuple] = (),
    tolerance: float | None = None,
) -> Result:
    """Simulate an instantiated FMU from start for the given number of steps and record its trajectory.

    fmu is an instantiated FMPy FMU2Slave (or anything with its methods). The
    order of the calls is part of the result, so it is fixed:

    1. setupExperiment, then the start values, then the inputs of the start time
    2. enter and exit the initialization mode
    3. at every communication point t_k: record, step to t_k+1, set the inputs of t_k+1
    4. after the last communication point: terminate

    Inputs are only set when another row of the input table starts to hold.
    A failing FMI call ends the simulation: the result holds the rows recorded
    so far and the error. The FMU is not terminated then.
    """
    times = [start + k * step for k in range(steps + 1)]
    result = Result(recorded, [], [])
    # Read the variables of one type with one call per row:
    # getter -> (value references, positions in the row)
    groups: dict[str, tuple[list, list]] = {}
    for index, variable in enumerate(recorded):
        references, indices = groups.setdefault(GETTERS[variable.type], ([], []))
        references.append(variable.valueReference)
        indices.append(index)
    active = None

    def apply_inputs(time: float) -> None:
        nonlocal active
        if inputs is None:
            return
        row = inputs.active_row(time, GRID_TOLERANCE * step)
        if row is not None and row != active:
            set_values(fmu, inputs.values(row))
            active = row

    def record(time: float) -> None:
        values = [None] * len(recorded)
        for getter, (references, indices) in groups.items():
            for index, value in zip(indices, getattr(fmu, getter)(references)):
                values[index] = (
                    bool(value) if recorded[index].type == "Boolean" else value
                )
        result.times.append(time)
        result.rows.append(values)

    phase = "initialization"
    try:
        fmu.setupExperiment(tolerance=tolerance, startTime=times[0], stopTime=times[-1])
        set_values(fmu, start_values)
        apply_inputs(times[0])
        fmu.enterInitializationMode()
        fmu.exitInitializationMode()
        for k, time in enumerate(times):
            record(time)
            if k == steps:
                break
            phase = f"step from t = {time!r}"
            fmu.doStep(currentCommunicationPoint=time, communicationStepSize=step)
            phase = f"inputs at t = {times[k + 1]!r}"
            apply_inputs(times[k + 1])
        phase = "termination"
        fmu.terminate()
    except FMICallException as error:
        result.error = f"{phase}: {error}"
    return result


def simulate(
    fmu_path: Path,
    *,
    start: float | None = None,
    stop: float | None = None,
    step: float | None = None,
    tolerance: float | None = None,
    input_file: Path | None = None,
    start_values: dict | None = None,
) -> Result:
    """Simulate an FMU file and return the recorded trajectory.

    start, stop and step default to the DefaultExperiment of the FMU. Without
    a tolerance the FMU uses the one it suggests itself. start_values maps
    variable names to values (or to texts, which are converted to the
    variable's type); they are set before the initialization.
    """
    model_description = fmpy.read_model_description(fmu_path)
    if model_description.coSimulation is None:
        raise SimulationError(f"{fmu_path} is not a co-simulation FMU")

    experiment = model_description.defaultExperiment

    def default(value: float | None, name: str) -> float:
        if value is not None:
            return value
        suggested = getattr(experiment, name, None) if experiment is not None else None
        if suggested is None:
            if name == "startTime":
                return 0.0
            option = {"stopTime": "--stop", "stepSize": "--step"}[name]
            raise SimulationError(f"the FMU suggests no {name}, give it with {option}")
        return float(suggested)

    start = default(start, "startTime")
    stop = default(stop, "stopTime")
    step = default(step, "stepSize")
    steps = number_of_steps(start, stop, step)

    by_name = {variable.name: variable for variable in model_description.modelVariables}
    values = []
    for name, value in (start_values or {}).items():
        if name not in by_name:
            raise SimulationError(f"the model has no variable named '{name}'")
        variable = by_name[name]
        values.append(
            (
                variable,
                parse_value(variable, value) if isinstance(value, str) else value,
            )
        )
    inputs = (
        read_inputs(input_file, model_description) if input_file is not None else None
    )

    identifier = model_description.coSimulation.modelIdentifier
    with tempfile.TemporaryDirectory(
        prefix="csfmu_", ignore_cleanup_errors=True
    ) as unzipdir:
        fmpy.extract(fmu_path, unzipdir)
        fmu = fmpy.fmi2.FMU2Slave(
            guid=model_description.guid,
            unzipDirectory=unzipdir,
            modelIdentifier=identifier,
            instanceName=identifier,
        )
        try:
            fmu.instantiate(loggingOn=False)
        # FMPy reports a failed fmi2Instantiate with a plain Exception (the FMU
        # has logged the reason already)
        except Exception as error:  # noqa: BLE001
            fmu.freeLibrary()
            raise SimulationError(f"cannot instantiate {fmu_path}: {error}") from None
        try:
            fmu.setDebugLogging(True, LOG_CATEGORIES)
            return run(
                fmu,
                recorded_variables(model_description),
                start,
                step,
                steps,
                inputs=inputs,
                start_values=values,
                tolerance=tolerance,
            )
        finally:
            # Also unloads the library, so the folder can be removed on Windows
            fmu.freeInstance()


def write_csv(path: Path, result: Result) -> None:
    """Write the result as CSV: a header with time and the variable names, then one row per communication point.

    The line ending is always \\n, so that the same trajectory gives the same
    file on every platform.
    """
    with open(path, "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(result.columns)
        for time, values in zip(result.times, result.rows):
            writer.writerow(
                [
                    repr(time),
                    *(
                        format_value(v, value)
                        for v, value in zip(result.variables, values)
                    ),
                ]
            )
