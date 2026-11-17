import pytest


def read(fmu, variable):
    """ Return the current value of a variable of any type. """
    getter = {"Real": fmu.getReal, "Integer": fmu.getInteger, "Boolean": fmu.getBoolean}[variable.type]
    return getter([variable.valueReference])[0]


def change(fmu, variable):
    """ Set a variable of any type to a value that differs from its current one. """
    value = read(fmu, variable)

    if variable.type == "Real":
        # Stay within the bounds of the variable
        step = -1.0 if variable.max is not None and value + 1.0 > float(variable.max) else 1.0
        fmu.setReal([variable.valueReference], [value + step])
    elif variable.type == "Integer":
        step = -1 if variable.max is not None and value + 1 > int(variable.max) else 1
        fmu.setInteger([variable.valueReference], [value + step])
    else:
        fmu.setBoolean([variable.valueReference], [not value])


def test_outputs_ignore_inputs_they_do_not_depend_on(initialized_fmu, model_description) -> None:
    """
    Verify the dependencies of the outputs in the modelDescription.xml
    against the behaviour of the FMU.

    An output that is declared not to depend on an input must not change
    when only this input is set. This is checked right after initialization
    and again after a step.
    """
    inputs = [variable for variable in model_description.modelVariables if variable.causality == "input"]

    # (output, input) pairs that are declared as independent
    independent = [
        (unknown.variable, variable)
        for unknown in model_description.outputs if unknown.dependencies is not None
        for variable in inputs if variable not in unknown.dependencies
    ]
    if not independent:
        pytest.skip("the model declares no output that is independent of an input")

    fmu = initialized_fmu

    for time in (0.0, 0.01):
        for output, input in independent:
            before = read(fmu, output)
            change(fmu, input)
            after = read(fmu, output)
            assert after == before, (
                f"output '{output.name}' changed from {before} to {after} when input '{input.name}' "
                f"was set at t={time}, but it is declared not to depend on it (dependsOn)"
            )

        fmu.doStep(currentCommunicationPoint=time, communicationStepSize=0.01)
