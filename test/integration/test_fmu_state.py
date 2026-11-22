"""Model-independent tests of FMU state handling.

Covers fmi2GetFMUstate, fmi2SetFMUstate, fmi2FreeFMUstate and serialization.
"""

import pytest
from fmpy.fmi1 import FMICallException

from test.fmu_helpers import initialize, read_all, variables_with


def excite(fmu, model_description) -> None:
    """Set every Real input to a non-zero value so that the model moves."""
    for variable in variables_with(model_description, "input", "Real"):
        fmu.setReal([variable.valueReference], [0.5])


def run(
    fmu, model_description, start: float, steps: int = 20, step_size: float = 0.01
) -> list:
    """Step the FMU and return the values of all variables after every step."""
    trajectory = []
    for k in range(steps):
        fmu.doStep(
            currentCommunicationPoint=start + k * step_size,
            communicationStepSize=step_size,
        )
        trajectory.append(read_all(fmu, model_description))
    return trajectory


def test_capability_flags_are_set(model_description) -> None:
    assert model_description.coSimulation.canGetAndSetFMUstate
    assert model_description.coSimulation.canSerializeFMUstate


def test_set_fmu_state_repeats_the_trajectory(
    initialized_fmu, model_description
) -> None:
    """Going back to a snapshot and stepping again gives exactly the same result."""
    excite(initialized_fmu, model_description)
    run(initialized_fmu, model_description, start=0.0, steps=5)

    state = initialized_fmu.getFMUstate()
    at_snapshot = read_all(initialized_fmu, model_description)
    first = run(initialized_fmu, model_description, start=0.05)

    initialized_fmu.setFMUstate(state)
    assert read_all(initialized_fmu, model_description) == at_snapshot
    second = run(initialized_fmu, model_description, start=0.05)

    assert first == second
    initialized_fmu.freeFMUstate(state)


def test_fmu_state_includes_the_state_machine(new_fmu) -> None:
    """A snapshot taken before initialization brings the FMU back to that point."""
    fmu = new_fmu()
    state = fmu.getFMUstate()

    initialize(fmu)
    fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.1)

    fmu.setFMUstate(state)

    # the FMU is in the instantiated state again
    initialize(fmu)
    fmu.freeFMUstate(state)


def test_set_fmu_state_recovers_from_an_error(initialized_fmu) -> None:
    """A snapshot is the way to continue after an error without a reset."""
    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.1)
    state = initialized_fmu.getFMUstate()

    with pytest.raises(FMICallException):
        initialized_fmu.enterInitializationMode()
    with pytest.raises(FMICallException):
        initialized_fmu.doStep(currentCommunicationPoint=0.1, communicationStepSize=0.1)

    initialized_fmu.setFMUstate(state)
    initialized_fmu.doStep(currentCommunicationPoint=0.1, communicationStepSize=0.1)
    initialized_fmu.freeFMUstate(state)


def test_cannot_get_fmu_state_in_error_state(initialized_fmu) -> None:
    with pytest.raises(FMICallException):
        initialized_fmu.enterInitializationMode()
    with pytest.raises(FMICallException):
        initialized_fmu.getFMUstate()


def test_existing_fmu_state_is_reused(initialized_fmu, model_description) -> None:
    """Passing an existing snapshot to fmi2GetFMUstate overwrites it."""
    excite(initialized_fmu, model_description)
    state = initialized_fmu.getFMUstate()
    address = state.value

    run(initialized_fmu, model_description, start=0.0, steps=5)
    expected = read_all(initialized_fmu, model_description)
    initialized_fmu.fmi2GetFMUstate(initialized_fmu.component, state)
    assert state.value == address

    run(initialized_fmu, model_description, start=0.05, steps=5)
    initialized_fmu.setFMUstate(state)

    assert read_all(initialized_fmu, model_description) == expected
    initialized_fmu.freeFMUstate(state)


def test_free_fmu_state_clears_the_pointer(initialized_fmu) -> None:
    state = initialized_fmu.getFMUstate()
    initialized_fmu.freeFMUstate(state)
    assert not state.value

    # freeing again (NULL) is harmless
    initialized_fmu.freeFMUstate(state)


def test_serialized_fmu_state_round_trip(
    initialized_fmu, new_fmu, model_description
) -> None:
    """A serialized state can be loaded into another instance of the same FMU."""
    excite(initialized_fmu, model_description)
    run(initialized_fmu, model_description, start=0.0, steps=5)

    state = initialized_fmu.getFMUstate()
    serialized = initialized_fmu.serializeFMUstate(state)
    initialized_fmu.freeFMUstate(state)
    expected = run(initialized_fmu, model_description, start=0.05)

    other = new_fmu("other_instance")
    restored = other.deSerializeFMUstate(serialized)
    other.setFMUstate(restored)
    other.freeFMUstate(restored)

    assert run(other, model_description, start=0.05) == expected


def test_serialization_is_deterministic(initialized_fmu) -> None:
    """Two snapshots of the same FMU state serialize to the same bytes."""
    first = initialized_fmu.getFMUstate()
    second = initialized_fmu.getFMUstate()

    assert initialized_fmu.serializeFMUstate(
        first
    ) == initialized_fmu.serializeFMUstate(second)

    initialized_fmu.freeFMUstate(first)
    initialized_fmu.freeFMUstate(second)


def test_deserialize_rejects_wrong_size(initialized_fmu) -> None:
    state = initialized_fmu.getFMUstate()
    serialized = initialized_fmu.serializeFMUstate(state)
    initialized_fmu.freeFMUstate(state)

    with pytest.raises(FMICallException):
        initialized_fmu.deSerializeFMUstate(serialized[:-1])


def test_deserialize_rejects_foreign_bytes(initialized_fmu) -> None:
    state = initialized_fmu.getFMUstate()
    serialized = initialized_fmu.serializeFMUstate(state)
    initialized_fmu.freeFMUstate(state)

    with pytest.raises(FMICallException):
        initialized_fmu.deSerializeFMUstate(bytes(len(serialized)))
