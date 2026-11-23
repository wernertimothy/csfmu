"""Tests of the thermal_mass model and its local variable Q_loss (ROADMAP P1.2).

    C*dT/dt = P - Q_loss,   Q_loss = (T - T_amb)/R,   T_out = T

Q_loss is a local variable: computed by the output equation, published and
readable, but it can never be set and is not an output. With the defaults
(C = 1000 J/K, R = 0.1 K/W) the time constant is tau = R*C = 100 s. From
T(0) = T_amb a constant heating power P gives

    T(t)      = T_amb + R*P*(1 - exp(-t/tau))
    Q_loss(t) = P*(1 - exp(-t/tau))

RK4 with maxStepSize = 1 s is accurate to about 1e-11 here.
"""

import math

import pytest
from fmpy.fmi1 import FMICallException

from tests.fmu_helpers import advance, initialize

STEP_SIZE = 1.0
POWER = 500.0


def real(fmu, vr, name: str) -> float:
    return fmu.getReal([vr(name)])[0]


def test_heat_loss_follows_the_step_response(initialized_fmu, vr) -> None:
    """After a step in P, T_out and Q_loss follow the analytic solution."""
    fmu = initialized_fmu
    R, C, T_amb = (real(fmu, vr, name) for name in ("R", "C", "T_amb"))
    fmu.setReal([vr("P")], [POWER])

    end = advance(fmu, 100.0, step_size=STEP_SIZE)

    rise = 1.0 - math.exp(-end / (R * C))
    assert real(fmu, vr, "T_out") == pytest.approx(T_amb + R * POWER * rise, rel=1e-9)
    assert real(fmu, vr, "Q_loss") == pytest.approx(POWER * rise, rel=1e-9)


def test_heat_loss_follows_a_parameter_set_without_a_step(initialized_fmu, vr) -> None:
    """Q_loss is recomputed as soon as T_amb is set, like an output."""
    fmu = initialized_fmu
    R = real(fmu, vr, "R")
    fmu.setReal([vr("P")], [POWER])
    advance(fmu, 10.0, step_size=STEP_SIZE)
    T = real(fmu, vr, "T_out")

    fmu.setReal([vr("T_amb")], [303.15])

    assert real(fmu, vr, "Q_loss") == pytest.approx((T - 303.15) / R, rel=1e-12)
    assert real(fmu, vr, "T_out") == T


def test_heat_loss_follows_the_initial_temperature(new_fmu, vr) -> None:
    """Setting the state before the initialization updates Q_loss at once."""
    fmu = new_fmu()
    R, T_amb = real(fmu, vr, "R"), real(fmu, vr, "T_amb")
    T0 = T_amb + R * POWER

    fmu.setReal([vr("T")], [T0])
    assert real(fmu, vr, "Q_loss") == pytest.approx(POWER, rel=1e-12)

    initialize(fmu)
    assert real(fmu, vr, "Q_loss") == pytest.approx(POWER, rel=1e-12)


def test_steady_state_holds(new_fmu, vr) -> None:
    """At T = T_amb + R*P the heat loss equals the heating power and T stays constant."""
    fmu = new_fmu()
    R, T_amb = real(fmu, vr, "R"), real(fmu, vr, "T_amb")
    T_ss = T_amb + R * POWER
    fmu.setReal([vr("T"), vr("P")], [T_ss, POWER])
    initialize(fmu)

    advance(fmu, 100.0, step_size=STEP_SIZE)

    assert real(fmu, vr, "T_out") == pytest.approx(T_ss, abs=1e-9)
    assert real(fmu, vr, "Q_loss") == pytest.approx(POWER, rel=1e-9)


def test_heat_loss_cannot_be_set(new_fmu, vr) -> None:
    """Setting the local variable is refused, and the log tells why."""
    log = []
    fmu = initialize(new_fmu(log=log))

    with pytest.raises(FMICallException):
        fmu.setReal([vr("Q_loss")], [1.0])

    assert any(
        "'Q_loss' is a local variable and cannot be set" in message
        for _, _, message in log
    ), log
