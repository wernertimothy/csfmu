"""Model-independent tests of the declared dependencies of the outputs (dependsOn)."""

import pytest

from tests.fmu_helpers import changed_value, read, variables_with, write


def change(fmu, variable) -> None:
    """Set a variable to a value that differs from its current one."""
    write(fmu, variable, changed_value(variable, read(fmu, variable)))


def test_outputs_ignore_inputs_they_do_not_depend_on(
    initialized_fmu, model_description
) -> None:
    """
    Verify the dependencies of the outputs in the modelDescription.xml
    against the behaviour of the FMU.

    An output that is declared not to depend on an input must not change
    when only this input is set. This is checked right after initialization
    and again after a step.
    """
    inputs = variables_with(model_description, "input")

    # (output, input) pairs that are declared as independent
    independent = [
        (unknown.variable, variable)
        for unknown in model_description.outputs
        if unknown.dependencies is not None
        for variable in inputs
        if variable not in unknown.dependencies
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


def test_outputs_follow_inputs_they_depend_on(
    initialized_fmu, model_description
) -> None:
    """
    Verify the declared direct feedthrough against the behaviour of the FMU.

    An output that declares a dependency on an input (dependsOn) must change
    right after this input is set, without a step in between. This is
    checked right after initialization and again after a step.

    FMI only says that such an output *may* depend on the input. The template
    is stricter: declare only real dependencies, and choose start values at
    which the dependency is active. An output without a declaration (all
    inputs) is a safe over-approximation and is not checked.
    """
    inputs = variables_with(model_description, "input")

    # (output, input) pairs that are declared as dependent
    dependent = [
        (unknown.variable, variable)
        for unknown in model_description.outputs
        if unknown.dependencies is not None
        for variable in inputs
        if variable in unknown.dependencies
    ]
    if not dependent:
        pytest.skip("the model declares no output that depends on an input")

    fmu = initialized_fmu

    for time in (0.0, 0.01):
        for output, input in dependent:
            before = read(fmu, output)
            change(fmu, input)
            after = read(fmu, output)
            assert after != before, (
                f"output '{output.name}' stayed at {before} when input '{input.name}' was set "
                f"at t={time}, but it is declared to depend on it (dependsOn): outputs must be "
                f"recomputed after a set"
            )

        fmu.doStep(currentCommunicationPoint=time, communicationStepSize=0.01)
