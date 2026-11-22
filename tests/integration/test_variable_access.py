"""Model-independent tests of variable access (fmi2GetXXX / fmi2SetXXX).

The tests discover the variables of the FMU from its modelDescription.xml,
so they work for every model built with this template.
"""

import pytest
from fmpy.fmi1 import FMICallException

from tests.fmu_helpers import (
    changed_value,
    initialize,
    read,
    settable_before_initialization,
    start_value,
    variables_with,
    write,
)


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


def test_variables_hold_start_values_after_instantiation(
    new_fmu, model_description
) -> None:
    """Right after instantiation every variable equals its start value."""
    fmu = new_fmu()
    for variable in model_description.modelVariables:
        if variable.start is not None:
            assert read(fmu, variable) == start_value(variable), variable.name


def test_values_set_before_initialization_survive(new_fmu, model_description) -> None:
    """
    Values set in the instantiated state must not be overwritten by
    fmi2EnterInitializationMode. This is how fmpy applies start_values.
    """
    fmu = new_fmu()
    settable = settable_before_initialization(model_description)
    expected = {}

    for variable in settable:
        expected[variable.name] = changed_value(variable)
        write(fmu, variable, expected[variable.name])

    initialize(fmu)

    for variable in settable:
        assert read(fmu, variable) == expected[variable.name], variable.name


def test_values_can_be_set_in_initialization_mode(new_fmu, model_description) -> None:
    """Parameters, inputs and states can be set during initialization mode."""
    fmu = new_fmu()
    settable = settable_before_initialization(model_description)
    expected = {}

    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    for variable in settable:
        expected[variable.name] = changed_value(variable)
        write(fmu, variable, expected[variable.name])
    fmu.exitInitializationMode()

    for variable in settable:
        assert read(fmu, variable) == expected[variable.name], variable.name


def test_inputs_can_be_set_between_steps(initialized_fmu, model_description) -> None:
    """Inputs can be set after initialization."""
    for variable in variables_with(model_description, "input"):
        value = changed_value(variable)
        write(initialized_fmu, variable, value)
        assert read(initialized_fmu, variable) == value, variable.name


def test_states_cannot_be_set_after_initialization(
    initialized_fmu, model_description
) -> None:
    """States are initial conditions: setting them during the simulation is an error."""
    state = variables_with(model_description, "local")[0]
    with pytest.raises(FMICallException):
        write(initialized_fmu, state, 1.0)


def test_outputs_cannot_be_set(new_fmu, model_description) -> None:
    """Outputs are computed by the model and can never be set."""
    output = variables_with(model_description, "output")[0]
    with pytest.raises(FMICallException):
        write(new_fmu(), output, True if output.type == "Boolean" else 1)


def test_access_with_wrong_type_fails(new_fmu, model_description) -> None:
    """A Real variable cannot be accessed through the Integer or Boolean functions."""
    real = next(v for v in model_description.modelVariables if v.type == "Real")

    with pytest.raises(FMICallException):
        new_fmu().getInteger([real.valueReference])
    with pytest.raises(FMICallException):
        new_fmu().getBoolean([real.valueReference])
    with pytest.raises(FMICallException):
        new_fmu().setInteger([real.valueReference], [1])
    with pytest.raises(FMICallException):
        new_fmu().setBoolean([real.valueReference], [True])


def test_access_with_unknown_value_reference_fails(new_fmu, model_description) -> None:
    """A value reference beyond the variable table is an error."""
    unknown = len(model_description.modelVariables)

    with pytest.raises(FMICallException):
        new_fmu().getReal([unknown])
    with pytest.raises(FMICallException):
        new_fmu().setReal([unknown], [1.0])


def test_string_access_fails(new_fmu) -> None:
    """The template has no String variables, so every String access is an error."""
    with pytest.raises(FMICallException):
        new_fmu().getString([0])
    with pytest.raises(FMICallException):
        new_fmu().setString([0], ["text"])


def test_reset_restores_start_values(initialized_fmu, model_description) -> None:
    """fmi2Reset brings every variable back to its start value."""
    for variable in variables_with(model_description, "input"):
        write(initialized_fmu, variable, changed_value(variable))
    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.1)

    initialized_fmu.reset()

    for variable in model_description.modelVariables:
        if variable.start is not None:
            assert read(initialized_fmu, variable) == start_value(variable), (
                variable.name
            )


def test_variables_can_be_read_in_initialization_mode(
    new_fmu, model_description
) -> None:
    """
    Every variable can be read during initialization mode. Outputs are
    computed from the values set so far, so they already equal the outputs
    at the first communication point.
    """
    fmu = new_fmu()
    outputs = variables_with(model_description, "output")

    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    for variable in settable_before_initialization(model_description):
        write(fmu, variable, changed_value(variable))
    during_initialization = [read(fmu, output) for output in outputs]
    fmu.exitInitializationMode()

    assert [read(fmu, output) for output in outputs] == during_initialization


def test_outputs_follow_values_set_before_initialization(
    new_fmu, model_description
) -> None:
    """Outputs read in the instantiated state reflect the values set so far."""
    fmu = new_fmu()
    outputs = variables_with(model_description, "output")

    for variable in settable_before_initialization(model_description):
        write(fmu, variable, changed_value(variable))
    before_initialization = [read(fmu, output) for output in outputs]

    initialize(fmu)

    assert [read(fmu, output) for output in outputs] == before_initialization
