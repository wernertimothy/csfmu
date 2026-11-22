import math

import pytest
from fmpy.fmi1 import FMICallException

"""
Tests of steps discarded because of an output, with the pi_controller model:

    e = r - y_m,  dx_I/dt = e,  u = Kp*e + Ki*x_I

A NaN gain Kp only reaches the output u, the state x_I stays finite. The
FMU must discard the step all the same and roll back to the communication
point.
"""

FMI2_DISCARD = 2

# fmi2StatusKind
FMI2_DO_STEP_STATUS = 0
FMI2_LAST_SUCCESSFUL_TIME = 2

STEP_SIZE = 0.1


@pytest.fixture(autouse=True)
def only_pi_controller(model_description) -> None:
    """Skip these tests if the FMU under test is not the pi_controller model."""
    if model_description.modelName != "pi_controller":
        pytest.skip(
            f"Test specific to pi_controller model, got {model_description.modelName}"
        )


def test_non_finite_output_discards_the_step(new_fmu, vr) -> None:
    """Kp = NaN only reaches the output u: the step is discarded although x_I stays finite."""
    log = []
    fmu = new_fmu(log=log)
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    fmu.setReal([vr("r")], [1.0])
    fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=STEP_SIZE)
    x_i = fmu.getReal([vr("x_I")])[0]

    fmu.setReal([vr("Kp")], [math.nan])
    with pytest.raises(FMICallException) as failure:
        fmu.doStep(currentCommunicationPoint=STEP_SIZE, communicationStepSize=STEP_SIZE)

    assert failure.value.status == FMI2_DISCARD
    assert fmu.getStatus(FMI2_DO_STEP_STATUS) == FMI2_DISCARD
    assert fmu.getRealStatus(FMI2_LAST_SUCCESSFUL_TIME) == STEP_SIZE
    assert fmu.getReal([vr("x_I")])[0] == x_i  # rolled back, bit-identical

    discards = [message for status, _, message in log if status == FMI2_DISCARD]
    assert len(discards) == 1, log
    assert "Output 'u'" in discards[0]
