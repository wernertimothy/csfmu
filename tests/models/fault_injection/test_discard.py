"""Tests of discarded steps with the fault_injection model.

    dx/dt = -x/tau,  y = x,  and dx/dt = NaN for t >= t_fail if fail is set

With the defaults (tau = 1, t_fail = 0.55, maxStepSize = 0.01) and a
communication step of 0.1, the step from 0.5 to 0.6 runs into the fault in
its 5th of 10 sub-steps. The FMU must discard the whole step and roll back to
t = 0.5, not to the last good sub-step.
"""

import math

import pytest
from fmpy.fmi1 import FMICallException
from fmpy.fmi2 import (
    fmi2Discard,
    fmi2DoStepStatus,
    fmi2Error,
    fmi2LastSuccessfulTime,
    fmi2OK,
    fmi2Terminated,
)

from tests.fmu_helpers import advance, initialize

STEP_SIZE = 0.1
FAILING_STEP_START = 0.5


def faulty_fmu(new_fmu, vr, log: list | None = None):
    """Return an initialized FMU with the fault switched on, stepped to t = 0.5."""
    fmu = new_fmu(log=log)
    fmu.setBoolean([vr("fail")], [True])
    initialize(fmu)
    advance(fmu, FAILING_STEP_START, step_size=STEP_SIZE)
    return fmu


def step_into_the_fault(fmu) -> FMICallException:
    """Do the step from 0.5 to 0.6, which must be discarded."""
    with pytest.raises(FMICallException) as failure:
        fmu.doStep(
            currentCommunicationPoint=FAILING_STEP_START,
            communicationStepSize=STEP_SIZE,
        )
    assert failure.value.status == fmi2Discard
    return failure.value


def test_discarded_step_rolls_back_to_the_communication_point(new_fmu, vr) -> None:
    """Snapshot -> step into the fault -> Discard -> restore -> disable the fault -> step."""
    fmu = faulty_fmu(new_fmu, vr)
    state = fmu.getFMUstate()
    y_before = fmu.getReal([vr("y")])[0]

    step_into_the_fault(fmu)

    # the master learns what happened and where the FMU stands
    assert fmu.getStatus(fmi2DoStepStatus) == fmi2Discard
    assert fmu.getRealStatus(fmi2LastSuccessfulTime) == FAILING_STEP_START
    assert not fmu.getBooleanStatus(fmi2Terminated)  # retry possible

    # the FMU is back at t = 0.5, not at the last good sub-step
    y = fmu.getReal([vr("y")])[0]
    assert y == y_before
    assert abs(y - math.exp(-FAILING_STEP_START)) < 1e-9

    # recover: go back to the snapshot, switch the fault off, step again
    fmu.setFMUstate(state)
    fmu.setBoolean([vr("fail")], [False])
    fmu.doStep(
        currentCommunicationPoint=FAILING_STEP_START, communicationStepSize=STEP_SIZE
    )

    assert fmu.getStatus(fmi2DoStepStatus) == fmi2OK
    assert abs(fmu.getReal([vr("y")])[0] - math.exp(-0.6)) < 1e-9
    fmu.freeFMUstate(state)


def test_retry_with_the_fault_moved_reaches_the_failing_step(new_fmu, vr) -> None:
    """The fault time is tunable: after restoring, the master can move it and continue."""
    fmu = faulty_fmu(new_fmu, vr)
    state = fmu.getFMUstate()
    step_into_the_fault(fmu)

    fmu.setFMUstate(state)
    fmu.setReal([vr("t_fail")], [0.65])
    fmu.doStep(
        currentCommunicationPoint=FAILING_STEP_START, communicationStepSize=STEP_SIZE
    )

    with pytest.raises(FMICallException) as failure:
        fmu.doStep(currentCommunicationPoint=0.6, communicationStepSize=STEP_SIZE)
    assert failure.value.status == fmi2Discard
    assert fmu.getRealStatus(fmi2LastSuccessfulTime) == 0.6
    fmu.freeFMUstate(state)


def test_step_is_refused_until_the_state_is_restored(new_fmu, vr) -> None:
    """fmi2DoStep is not allowed after a Discard. It is an error, which a snapshot still repairs."""
    fmu = faulty_fmu(new_fmu, vr)
    state = fmu.getFMUstate()
    step_into_the_fault(fmu)

    with pytest.raises(FMICallException) as failure:
        fmu.doStep(
            currentCommunicationPoint=FAILING_STEP_START,
            communicationStepSize=STEP_SIZE / 2,
        )
    assert failure.value.status == fmi2Error

    fmu.setFMUstate(state)
    fmu.setBoolean([vr("fail")], [False])
    fmu.doStep(
        currentCommunicationPoint=FAILING_STEP_START, communicationStepSize=STEP_SIZE
    )
    fmu.freeFMUstate(state)


def test_variables_cannot_be_set_after_a_discard(new_fmu, vr) -> None:
    """FMI 2.0 allows no fmi2SetXXX in the step failed state."""
    fmu = faulty_fmu(new_fmu, vr)
    step_into_the_fault(fmu)

    with pytest.raises(FMICallException) as failure:
        fmu.setBoolean([vr("fail")], [False])
    assert failure.value.status == fmi2Error


def test_terminate_is_allowed_after_a_discard(new_fmu, vr) -> None:
    fmu = faulty_fmu(new_fmu, vr)
    step_into_the_fault(fmu)

    fmu.terminate()


def test_reset_is_allowed_after_a_discard(new_fmu, vr) -> None:
    """After a reset the fault is off again (start value) and the simulation runs through."""
    fmu = faulty_fmu(new_fmu, vr)
    step_into_the_fault(fmu)

    fmu.reset()
    initialize(fmu)
    advance(fmu, 1.0, step_size=STEP_SIZE)


def test_discard_is_logged(new_fmu, vr) -> None:
    """The reason is logged as fmi2Discard in logStatusWarning, naming the state and the times."""
    log = []
    fmu = faulty_fmu(new_fmu, vr, log=log)
    step_into_the_fault(fmu)

    discards = [entry for entry in log if entry[0] == fmi2Discard]
    assert len(discards) == 1, log
    _, category, message = discards[0]
    assert category == "logStatusWarning"
    assert "'x'" in message
    assert "rolled back to 0.5" in message


def test_snapshot_taken_after_a_discard_keeps_the_failed_state(new_fmu, vr) -> None:
    """fmi2GetFMUstate is allowed after a Discard. Restoring it brings back the failed state."""
    fmu = faulty_fmu(new_fmu, vr)
    before = fmu.getFMUstate()
    step_into_the_fault(fmu)
    failed = fmu.getFMUstate()

    fmu.setFMUstate(before)
    assert fmu.getStatus(fmi2DoStepStatus) == fmi2OK

    fmu.setFMUstate(failed)
    assert fmu.getStatus(fmi2DoStepStatus) == fmi2Discard
    assert fmu.getRealStatus(fmi2LastSuccessfulTime) == FAILING_STEP_START

    fmu.freeFMUstate(before)
    fmu.freeFMUstate(failed)
