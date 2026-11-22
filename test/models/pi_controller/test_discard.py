"""Tests of steps discarded because of an output, with the pi_controller model.

    e = r - y_m,  dx_I/dt = e,  u = Kp*e + Ki*x_I

A NaN gain Kp only reaches the output u, the state x_I stays finite. The
FMU must discard the step all the same and roll back to the communication
point.
"""

import math

import pytest
from fmpy.fmi1 import FMICallException
from fmpy.fmi2 import fmi2Discard, fmi2DoStepStatus, fmi2LastSuccessfulTime

from test.fmu_helpers import initialize

STEP_SIZE = 0.1


def test_non_finite_output_discards_the_step(new_fmu, vr) -> None:
    """Kp = NaN only reaches the output u: the step is discarded although x_I stays finite."""
    log = []
    fmu = initialize(new_fmu(log=log))
    fmu.setReal([vr("r")], [1.0])
    fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=STEP_SIZE)
    x_i = fmu.getReal([vr("x_I")])[0]

    fmu.setReal([vr("Kp")], [math.nan])
    with pytest.raises(FMICallException) as failure:
        fmu.doStep(currentCommunicationPoint=STEP_SIZE, communicationStepSize=STEP_SIZE)

    assert failure.value.status == fmi2Discard
    assert fmu.getStatus(fmi2DoStepStatus) == fmi2Discard
    assert fmu.getRealStatus(fmi2LastSuccessfulTime) == STEP_SIZE
    assert fmu.getReal([vr("x_I")])[0] == x_i  # rolled back, bit-identical

    discards = [message for status, _, message in log if status == fmi2Discard]
    assert len(discards) == 1, log
    assert "Output 'u'" in discards[0]
