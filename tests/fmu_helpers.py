"""Helpers shared by the FMPy tests: typed variable access, initialization and stepping.

Status codes and status kinds are not defined here, they come from fmpy.fmi2
(fmi2OK, fmi2Discard, fmi2DoStepStatus, fmi2LastSuccessfulTime, ...).
"""

import ctypes

import fmpy.fmi2

# Enumeration variables are accessed like Integer variables (FMI 2.0)
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
}


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
    if variable.type in ("Integer", "Enumeration"):
        return int(variable.start)
    return float(variable.start)


def enumeration_values(variable) -> list[int]:
    """The values of the items of an enumeration variable, in the order of the items."""
    return [int(item.value) for item in variable.declaredType.items]


def enumeration_variables(model_description) -> list:
    """The variables whose type is an enumeration."""
    return [v for v in model_description.modelVariables if v.type == "Enumeration"]


def changed_value(variable, value=None):
    """A value that differs from value (default: the start value) and respects max.

    Real variables change by 0.25, Integer variables by 1, Boolean variables
    flip, Enumeration variables take the value of the next item (cyclic).
    """
    if value is None:
        value = start_value(variable)
    if variable.type == "Boolean":
        return not value
    if variable.type == "Enumeration":
        values = enumeration_values(variable)
        return values[(values.index(value) + 1) % len(values)]
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
    """The states: local variables with a start value (initial="exact" or "approx").

    States and local variables share causality="local"; initial tells them apart.
    """
    return [
        v
        for v in model_description.modelVariables
        if v.causality == "local" and v.initial in ("exact", "approx")
    ]


def approx_states(model_description) -> list:
    """The states whose start value the initialization may replace (initial="approx")."""
    return [v for v in states(model_description) if v.initial == "approx"]


def calculated_parameters(model_description) -> list:
    """The calculated parameters: computed from the parameters, read-only."""
    return variables_with(model_description, "calculatedParameter")


def local_variables(model_description) -> list:
    """The local variables: computed by the model (initial="calculated"), read-only."""
    return [
        v
        for v in model_description.modelVariables
        if v.causality == "local" and v.initial == "calculated"
    ]


def settable_before_initialization(model_description) -> list:
    """The variables the integration tool may set before fmi2EnterInitializationMode.

    These are the parameters, the inputs and all states, but not the local
    variables and the calculated parameters (FMI 2.0, section 4.2.4).
    """
    settable = {id(v) for v in states(model_description)}
    return [
        v
        for v in model_description.modelVariables
        if v.causality in ("parameter", "input") or id(v) in settable
    ]


def settable_in_initialization_mode(model_description) -> list:
    """The variables the integration tool may set in initialization mode.

    These are the parameters, the inputs and the states with initial="exact"
    (FMI 2.0, section 4.2.4): a state with initial="approx" can only be set before.
    """
    approx = {id(v) for v in approx_states(model_description)}
    return [
        v
        for v in settable_before_initialization(model_description)
        if id(v) not in approx
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
