"""Model-independent tests of variable access (fmi2GetXXX / fmi2SetXXX).

The tests discover the variables of the FMU from its modelDescription.xml,
so they work for every model built with this template.
"""

import pytest
from fmpy.fmi1 import FMICallException

from tests.fmu_helpers import (
    MAX_STRING_BYTES,
    approx_states,
    calculated_parameters,
    changed_value,
    enumeration_values,
    enumeration_variables,
    initialize,
    local_variables,
    read,
    settable_before_initialization,
    settable_in_initialization_mode,
    start_value,
    states,
    variables_with,
    write,
)


def test_value_references_are_unique(model_description) -> None:
    """Every variable has its own value reference (index in the variable table)."""
    value_references = [v.valueReference for v in model_description.modelVariables]
    assert sorted(value_references) == list(range(len(value_references)))


def test_states_are_exposed(model_description) -> None:
    """The states are published as continuous Real local variables with a start value.

    That every state is published is checked by the build (fmi2ModelDescription.c).
    """
    published = states(model_description)
    if not published:
        pytest.skip("the model has no states")
    for state in published:
        assert state.type == "Real"
        assert state.variability == "continuous"
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

    A state with initial="approx" is only a guess: the initialization may
    replace it, so it is set but not expected to survive.
    """
    fmu = new_fmu()
    settable = settable_before_initialization(model_description)
    approx = {v.name for v in approx_states(model_description)}
    expected = {}

    for variable in settable:
        expected[variable.name] = changed_value(variable)
        write(fmu, variable, expected[variable.name])

    initialize(fmu)

    for variable in settable:
        if variable.name not in approx:
            assert read(fmu, variable) == expected[variable.name], variable.name


def test_values_can_be_set_in_initialization_mode(new_fmu, model_description) -> None:
    """Parameters, inputs and states with initial="exact" can be set during initialization mode."""
    fmu = new_fmu()
    settable = settable_in_initialization_mode(model_description)
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
    published = states(model_description)
    if not published:
        pytest.skip("the model has no states")
    with pytest.raises(FMICallException):
        write(initialized_fmu, published[0], 1.0)


def test_approx_states_cannot_be_set_in_initialization_mode(
    new_fmu, model_description
) -> None:
    """A state with initial="approx" can only be set before the initialization (FMI 2.0, 4.2.4)."""
    approx = approx_states(model_description)
    if not approx:
        pytest.skip("the model has no state with initial=approx")

    for variable in approx:
        fmu = new_fmu()
        write(fmu, variable, changed_value(variable))  # allowed when instantiated
        fmu.setupExperiment(startTime=0.0)
        fmu.enterInitializationMode()
        with pytest.raises(FMICallException):
            write(fmu, variable, changed_value(variable))


def test_enumeration_start_values_are_items(model_description) -> None:
    """An enumeration variable with a start value starts at the value of an item."""
    for variable in enumeration_variables(model_description):
        assert variable.declaredType.items, variable.name
        if variable.start is not None:
            assert start_value(variable) in enumeration_values(variable), variable.name


def test_enumerations_refuse_values_that_are_no_item(
    new_fmu, model_description
) -> None:
    """Only the values of the items of an enumeration can be set."""
    settable = [
        v
        for v in enumeration_variables(model_description)
        if v.causality in ("parameter", "input")
    ]
    if not settable:
        pytest.skip("the model has no enumeration parameters or inputs")

    for variable in settable:
        fmu = new_fmu()
        with pytest.raises(FMICallException):
            write(fmu, variable, max(enumeration_values(variable)) + 1)


def test_calculated_parameters_cannot_be_set(new_fmu, model_description) -> None:
    """Calculated parameters are computed by the model and can never be set."""
    calculated = calculated_parameters(model_description)
    if not calculated:
        pytest.skip("the model has no calculated parameters")

    for variable in calculated:
        fmu = new_fmu()
        with pytest.raises(FMICallException):
            write(fmu, variable, changed_value(variable, read(fmu, variable)))


def test_calculated_parameters_are_initial_unknowns(model_description) -> None:
    """Calculated parameters are published as initial unknowns (FMI 2.0, section 2.2.8)."""
    calculated = calculated_parameters(model_description)
    if not calculated:
        pytest.skip("the model has no calculated parameters")

    initial_unknowns = {
        unknown.variable.name for unknown in model_description.initialUnknowns
    }
    for variable in calculated:
        assert variable.initial == "calculated", variable.name
        assert variable.start is None, variable.name
        assert variable.name in initial_unknowns, variable.name


def test_local_variables_have_no_start_value(model_description) -> None:
    """Local variables are computed by the model, so they are published without start value."""
    for variable in local_variables(model_description):
        assert variable.initial == "calculated", variable.name
        assert variable.start is None, variable.name


@pytest.mark.parametrize("initialized", [False, True], ids=["instantiated", "stepping"])
def test_local_variables_cannot_be_set(new_fmu, model_description, initialized) -> None:
    """Local variables are computed by the model and can never be set."""
    locals_ = local_variables(model_description)
    if not locals_:
        pytest.skip("the model has no local variables")

    for variable in locals_:
        fmu = new_fmu()
        if initialized:
            initialize(fmu)
        with pytest.raises(FMICallException):
            write(fmu, variable, changed_value(variable, read(fmu, variable)))


def test_outputs_cannot_be_set(new_fmu, model_description) -> None:
    """Outputs are computed by the model and can never be set."""
    output = variables_with(model_description, "output")[0]
    with pytest.raises(FMICallException):
        write(new_fmu(), output, True if output.type == "Boolean" else 1)


def test_access_with_wrong_type_fails(new_fmu, model_description) -> None:
    """A Real variable cannot be accessed through the Integer, Boolean or String functions."""
    real = next(v for v in model_description.modelVariables if v.type == "Real")

    with pytest.raises(FMICallException):
        new_fmu().getInteger([real.valueReference])
    with pytest.raises(FMICallException):
        new_fmu().getBoolean([real.valueReference])
    with pytest.raises(FMICallException):
        new_fmu().getString([real.valueReference])
    with pytest.raises(FMICallException):
        new_fmu().setInteger([real.valueReference], [1])
    with pytest.raises(FMICallException):
        new_fmu().setBoolean([real.valueReference], [True])
    with pytest.raises(FMICallException):
        new_fmu().setString([real.valueReference], ["text"])


def test_access_with_unknown_value_reference_fails(new_fmu, model_description) -> None:
    """A value reference beyond the variable table is an error."""
    unknown = len(model_description.modelVariables)

    with pytest.raises(FMICallException):
        new_fmu().getReal([unknown])
    with pytest.raises(FMICallException):
        new_fmu().setReal([unknown], [1.0])
    with pytest.raises(FMICallException):
        new_fmu().getString([unknown])
    with pytest.raises(FMICallException):
        new_fmu().setString([unknown], ["text"])


def test_string_parameters_refuse_values_that_do_not_fit(
    new_fmu, model_description
) -> None:
    """A String parameter holds at most MAX_STRING_BYTES bytes; a longer value is
    refused, never cut, and the parameter keeps its value."""
    strings = variables_with(model_description, "parameter", "String")
    if not strings:
        pytest.skip("the model has no String parameters")

    for variable in strings:
        fmu = new_fmu()
        longest = "a" * MAX_STRING_BYTES
        write(fmu, variable, longest)
        assert read(fmu, variable) == longest, variable.name

        with pytest.raises(FMICallException):
            write(fmu, variable, "b" * (MAX_STRING_BYTES + 1))
        assert read(fmu, variable) == longest, variable.name


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

    Unless the model has states with initial="approx": their initialization
    at the end of initialization mode may move them, and the outputs with them.
    """
    fmu = new_fmu()
    outputs = variables_with(model_description, "output")

    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    for variable in settable_in_initialization_mode(model_description):
        write(fmu, variable, changed_value(variable))
    during_initialization = [read(fmu, output) for output in outputs]
    fmu.exitInitializationMode()

    after_initialization = [read(fmu, output) for output in outputs]
    if not approx_states(model_description):
        assert after_initialization == during_initialization


def test_outputs_follow_values_set_before_initialization(
    new_fmu, model_description
) -> None:
    """Outputs read in the instantiated state reflect the values set so far.

    Unless the model has states with initial="approx", which the
    initialization may move (see test_variables_can_be_read_in_initialization_mode).
    """
    fmu = new_fmu()
    outputs = variables_with(model_description, "output")

    for variable in settable_before_initialization(model_description):
        write(fmu, variable, changed_value(variable))
    before_initialization = [read(fmu, output) for output in outputs]

    initialize(fmu)

    after_initialization = [read(fmu, output) for output in outputs]
    if not approx_states(model_description):
        assert after_initialization == before_initialization
