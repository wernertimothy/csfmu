import pytest
from fmpy.validation import validate_fmu
from pathlib import Path

"""
Model-independent tests of variable access (fmi2GetXXX / fmi2SetXXX).

The tests discover the variables of the FMU from its modelDescription.xml,
so they work for every model built with this template.
"""

GETTERS = {"Real": "getReal", "Integer": "getInteger", "Boolean": "getBoolean"}
SETTERS = {"Real": "setReal", "Integer": "setInteger", "Boolean": "setBoolean"}


def get_value(fmu, variable):
    """Read a variable with the getter that matches its type."""
    return getattr(fmu, GETTERS[variable.type])([variable.valueReference])[0]


def set_value(fmu, variable, value) -> None:
    """Write a variable with the setter that matches its type."""
    getattr(fmu, SETTERS[variable.type])([variable.valueReference], [value])


def start_value(variable):
    """Start value from the modelDescription.xml, converted to a Python value."""
    if variable.type == "Boolean":
        return variable.start == "true"
    if variable.type == "Integer":
        return int(variable.start)
    return float(variable.start)


def changed_value(variable):
    """A value that differs from the start value and respects min and max."""
    start = start_value(variable)
    if variable.type == "Boolean":
        return not start
    step = 1 if variable.type == "Integer" else 0.25
    if variable.max is not None and start + step > float(variable.max):
        return start - step
    return start + step


def variables_with(model_description, causality: str, type: str | None = None) -> list:
    return [v for v in model_description.modelVariables
            if v.causality == causality and (type is None or v.type == type)]


def test_generated_model_description_is_valid(path_to_fmu: Path) -> None:
    """The generated modelDescription.xml passes fmpy's validation."""
    problems = validate_fmu(str(path_to_fmu))
    assert problems == [], f"modelDescription.xml is not valid: {problems}"


def test_value_references_are_unique(model_description) -> None:
    """Every variable has its own value reference (index in the variable table)."""
    value_references = [v.valueReference for v in model_description.modelVariables]
    assert sorted(value_references) == list(range(len(value_references)))


def test_states_are_exposed(model_description) -> None:
    """Every model publishes its states as local variables with a start value."""
    states = variables_with(model_description, "local")
    assert len(states) > 0
    for state in states:
        assert state.type == "Real"
        assert state.initial == "exact"
        assert state.start is not None


def test_variables_hold_start_values_after_instantiation(new_fmu, model_description) -> None:
    """Right after instantiation every variable equals its start value."""
    fmu = new_fmu()
    for variable in model_description.modelVariables:
        if variable.start is not None:
            assert get_value(fmu, variable) == start_value(variable), variable.name


def test_values_set_before_initialization_survive(new_fmu, model_description) -> None:
    """
    Values set in the instantiated state must not be overwritten by
    fmi2EnterInitializationMode. This is how fmpy applies start_values.
    """
    fmu = new_fmu()
    settable = [v for v in model_description.modelVariables if v.causality in ("parameter", "input", "local")]
    expected = {}

    for variable in settable:
        expected[variable.name] = changed_value(variable)
        set_value(fmu, variable, expected[variable.name])

    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    for variable in settable:
        assert get_value(fmu, variable) == expected[variable.name], variable.name


def test_values_can_be_set_in_initialization_mode(new_fmu, model_description) -> None:
    """Parameters, inputs and states can be set during initialization mode."""
    fmu = new_fmu()
    settable = [v for v in model_description.modelVariables if v.causality in ("parameter", "input", "local")]
    expected = {}

    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    for variable in settable:
        expected[variable.name] = changed_value(variable)
        set_value(fmu, variable, expected[variable.name])
    fmu.exitInitializationMode()

    for variable in settable:
        assert get_value(fmu, variable) == expected[variable.name], variable.name


def test_inputs_can_be_set_between_steps(initialized_fmu, model_description) -> None:
    """Inputs can be set after initialization."""
    for variable in variables_with(model_description, "input"):
        value = changed_value(variable)
        set_value(initialized_fmu, variable, value)
        assert get_value(initialized_fmu, variable) == value, variable.name


def test_states_cannot_be_set_after_initialization(initialized_fmu, model_description) -> None:
    """States are initial conditions: setting them during the simulation is an error."""
    state = variables_with(model_description, "local")[0]
    with pytest.raises(Exception):
        set_value(initialized_fmu, state, 1.0)


def test_outputs_cannot_be_set(new_fmu, model_description) -> None:
    """Outputs are computed by the model and can never be set."""
    output = variables_with(model_description, "output")[0]
    with pytest.raises(Exception):
        set_value(new_fmu(), output, True if output.type == "Boolean" else 1)


def test_access_with_wrong_type_fails(new_fmu, model_description) -> None:
    """A Real variable cannot be accessed through the Integer or Boolean functions."""
    real = [v for v in model_description.modelVariables if v.type == "Real"][0]

    with pytest.raises(Exception):
        new_fmu().getInteger([real.valueReference])
    with pytest.raises(Exception):
        new_fmu().getBoolean([real.valueReference])
    with pytest.raises(Exception):
        new_fmu().setInteger([real.valueReference], [1])
    with pytest.raises(Exception):
        new_fmu().setBoolean([real.valueReference], [True])


def test_access_with_unknown_value_reference_fails(new_fmu, model_description) -> None:
    """A value reference beyond the variable table is an error."""
    unknown = len(model_description.modelVariables)

    with pytest.raises(Exception):
        new_fmu().getReal([unknown])
    with pytest.raises(Exception):
        new_fmu().setReal([unknown], [1.0])


def test_string_access_fails(new_fmu) -> None:
    """The template has no String variables, so every String access is an error."""
    with pytest.raises(Exception):
        new_fmu().getString([0])


def test_reset_restores_start_values(initialized_fmu, model_description) -> None:
    """fmi2Reset brings every variable back to its start value."""
    for variable in variables_with(model_description, "input"):
        set_value(initialized_fmu, variable, changed_value(variable))
    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.1)

    initialized_fmu.reset()

    for variable in model_description.modelVariables:
        if variable.start is not None:
            assert get_value(initialized_fmu, variable) == start_value(variable), variable.name


def test_variables_can_be_read_in_initialization_mode(new_fmu, model_description) -> None:
    """
    Every variable can be read during initialization mode. Outputs are
    computed from the values set so far, so they already equal the outputs
    at the first communication point.
    """
    fmu = new_fmu()
    settable = [v for v in model_description.modelVariables if v.causality in ("parameter", "input", "local")]
    outputs = variables_with(model_description, "output")

    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    for variable in settable:
        set_value(fmu, variable, changed_value(variable))
    during_initialization = [get_value(fmu, output) for output in outputs]
    fmu.exitInitializationMode()

    assert [get_value(fmu, output) for output in outputs] == during_initialization


def test_outputs_follow_values_set_before_initialization(new_fmu, model_description) -> None:
    """Outputs read in the instantiated state reflect the values set so far."""
    fmu = new_fmu()
    settable = [v for v in model_description.modelVariables if v.causality in ("parameter", "input", "local")]
    outputs = variables_with(model_description, "output")

    for variable in settable:
        set_value(fmu, variable, changed_value(variable))
    before_initialization = [get_value(fmu, output) for output in outputs]

    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    assert [get_value(fmu, output) for output in outputs] == before_initialization
