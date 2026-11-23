"""Helpers shared by the FMPy tests: typed variable access, initialization and stepping.

Status codes and status kinds are not defined here, they come from fmpy.fmi2
(fmi2OK, fmi2Discard, fmi2DoStepStatus, fmi2LastSuccessfulTime, ...).
"""

import ctypes

import fmpy.fmi2

GETTERS = {"Real": "getReal", "Integer": "getInteger", "Boolean": "getBoolean"}
SETTERS = {"Real": "setReal", "Integer": "setInteger", "Boolean": "setBoolean"}


def read(fmu, variable):
    """Read a variable with the getter that matches its type."""
    return getattr(fmu, GETTERS[variable.type])([variable.valueReference])[0]


def write(fmu, variable, value) -> None:
    """Write a variable with the setter that matches its type."""
    getattr(fmu, SETTERS[variable.type])([variable.valueReference], [value])


def read_all(fmu, model_description) -> dict:
    """Read every variable of the FMU, keyed by name."""
    return {v.name: read(fmu, v) for v in model_description.modelVariables}


def start_value(variable):
    """Start value from the modelDescription.xml, converted to a Python value."""
    if variable.type == "Boolean":
        return variable.start == "true"
    if variable.type == "Integer":
        return int(variable.start)
    return float(variable.start)


def changed_value(variable, value=None):
    """A value that differs from value (default: the start value) and respects max.

    Real variables change by 0.25, Integer variables by 1, Boolean variables flip.
    """
    if value is None:
        value = start_value(variable)
    if variable.type == "Boolean":
        return not value
    step = 1 if variable.type == "Integer" else 0.25
    if variable.max is not None and value + step > float(variable.max):
        return value - step
    return value + step


def variables_with(model_description, causality: str, type: str | None = None) -> list:
    """The variables of a causality, optionally only those of one type."""
    return [
        v
        for v in model_description.modelVariables
        if v.causality == causality and (type is None or v.type == type)
    ]


def states(model_description) -> list:
    """The states: local variables with initial="exact", which can be set before the simulation.

    States and local variables share causality="local"; initial tells them apart.
    """
    return [
        v
        for v in model_description.modelVariables
        if v.causality == "local" and v.initial == "exact"
    ]


def local_variables(model_description) -> list:
    """The local variables: computed by the model (initial="calculated"), read-only."""
    return [
        v
        for v in model_description.modelVariables
        if v.causality == "local" and v.initial == "calculated"
    ]


def settable_before_initialization(model_description) -> list:
    """The variables the integration tool may set up to fmi2ExitInitializationMode.

    These are the parameters, the inputs and the states, but not the local variables.
    """
    settable = {id(v) for v in states(model_description)}
    return [
        v
        for v in model_description.modelVariables
        if v.causality in ("parameter", "input") or id(v) in settable
    ]


def initialize(fmu, **setup):
    """Run fmi2SetupExperiment, fmi2EnterInitializationMode and fmi2ExitInitializationMode.

    The keyword arguments go to setupExperiment (startTime defaults to 0).
    Returns the FMU.
    """
    setup.setdefault("startTime", 0.0)
    fmu.setupExperiment(**setup)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    return fmu


def advance(fmu, duration: float, step_size: float = 0.01, start: float = 0.0) -> float:
    """Do communication steps of step_size from start for duration; return the end time.

    The communication points are start + k*step_size.
    """
    steps = round(duration / step_size)
    for k in range(steps):
        fmu.doStep(
            currentCommunicationPoint=start + k * step_size,
            communicationStepSize=step_size,
        )
    return start + steps * step_size


def do_step_status(fmu, time: float, step_size: float) -> int:
    """fmi2DoStep returning its status.

    FMU2Slave.doStep drops the status and raises only for statuses above
    fmi2Warning, so the low-level call is used.
    """
    return fmu.fmi2DoStep(fmu.component, time, step_size, fmpy.fmi2.fmi2True)


def set_debug_logging(fmu, categories: list[str]) -> int:
    """fmi2SetDebugLogging(fmi2True, categories) returning its status.

    FMU2Slave.setDebugLogging drops the status, so the low-level call is used.
    """
    names = (ctypes.c_char_p * len(categories))(*(c.encode() for c in categories))
    return fmu.fmi2SetDebugLogging(
        fmu.component, fmpy.fmi2.fmi2True, len(categories), names
    )
