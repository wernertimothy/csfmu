"""Tests of the initialization of the pi_controller model.

    e = r - y_m,  dx_I/dt = e,  u = Kp*e + Ki*x_I

A NaN gain Kp only reaches the output u. The master would start from a NaN
output and every step would be discarded (see test_discard.py), so the
initialization fails instead.
"""

import math

import pytest
from fmpy.fmi1 import FMICallException
from fmpy.fmi2 import fmi2Error

from tests.fmu_helpers import initialize


def test_non_finite_output_fails_the_initialization(new_fmu, vr) -> None:
    """Kp = NaN and r = 1 make u NaN at the start: fmi2ExitInitializationMode fails."""
    log = []
    fmu = new_fmu(log=log)
    fmu.setReal([vr("Kp"), vr("r")], [math.nan, 1.0])

    with pytest.raises(FMICallException) as failure:
        initialize(fmu)

    assert failure.value.status == fmi2Error
    assert any(
        "Initialization failed: Output 'u' is not finite." in message
        for _, _, message in log
    ), log
