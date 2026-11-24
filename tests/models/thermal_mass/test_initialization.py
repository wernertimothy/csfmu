"""Tests of the initialization of the thermal_mass model (ROADMAP P1.3, P1.4).

    tau = R*C                              (calculated parameter)
    init_mode = fixed_start (1):  T(0) = start value of T
    init_mode = steady_state (2): T(0) = T_amb + R*P, so that dT/dt = 0

init_mode is an enumeration (InitMode), set and read like an Integer.

T is a state with initial="approx": the initialization may replace its start
value, so it can only be set before fmi2EnterInitializationMode.
"""

import pytest
from fmpy.fmi1 import FMICallException

from tests.fmu_helpers import advance, initialize

FIXED_START = 1
STEADY_STATE = 2
POWER = 500.0


def real(fmu, vr, name: str) -> float:
    return fmu.getReal([vr(name)])[0]


def test_tau_is_r_times_c_in_every_state(new_fmu, vr) -> None:
    """The calculated parameter follows R and C before, during and after the initialization."""
    fmu = new_fmu()
    assert real(fmu, vr, "tau") == pytest.approx(
        real(fmu, vr, "R") * real(fmu, vr, "C"), rel=1e-15
    )
    assert real(fmu, vr, "tau") == pytest.approx(100.0, rel=1e-15)

    fmu.setReal([vr("R")], [0.2])
    assert real(fmu, vr, "tau") == pytest.approx(200.0, rel=1e-15)

    initialize(fmu)
    assert real(fmu, vr, "tau") == pytest.approx(200.0, rel=1e-15)


def test_steady_state_starts_in_equilibrium_and_stays_there(new_fmu, vr) -> None:
    """init_mode = steady_state: T(0) = T_amb + R*P, and T does not move."""
    fmu = new_fmu()
    R, T_amb = real(fmu, vr, "R"), real(fmu, vr, "T_amb")
    fmu.setInteger([vr("init_mode")], [STEADY_STATE])
    fmu.setReal([vr("P")], [POWER])

    initialize(fmu)

    T_ss = T_amb + R * POWER  # 343.15 K
    assert real(fmu, vr, "T_out") == pytest.approx(T_ss, abs=1e-12)
    assert real(fmu, vr, "Q_loss") == pytest.approx(POWER, rel=1e-12)

    advance(fmu, 100.0, step_size=1.0)

    assert real(fmu, vr, "T_out") == pytest.approx(T_ss, abs=1e-9)
    assert real(fmu, vr, "Q_loss") == pytest.approx(POWER, rel=1e-9)


def test_steady_state_replaces_the_start_value(new_fmu, vr) -> None:
    """A start value set for T is only a guess (initial="approx") in steady_state mode."""
    fmu = new_fmu()
    fmu.setReal([vr("T")], [300.0])
    fmu.setInteger([vr("init_mode")], [STEADY_STATE])
    fmu.setReal([vr("P")], [POWER])

    initialize(fmu)

    assert real(fmu, vr, "T_out") == pytest.approx(343.15, abs=1e-12)


def test_fixed_start_keeps_the_start_value(new_fmu, vr) -> None:
    """init_mode = fixed_start (the default): T starts at the value set before."""
    fmu = new_fmu()
    assert fmu.getInteger([vr("init_mode")])[0] == FIXED_START
    fmu.setReal([vr("T")], [300.0])
    fmu.setReal([vr("P")], [POWER])

    initialize(fmu)

    assert real(fmu, vr, "T_out") == 300.0


def test_init_mode_is_an_enumeration(model_description) -> None:
    """init_mode is published as the enumeration InitMode with its two items."""
    init_mode = next(
        v for v in model_description.modelVariables if v.name == "init_mode"
    )

    assert init_mode.type == "Enumeration"
    assert init_mode.declaredType.name == "InitMode"
    assert {item.name: int(item.value) for item in init_mode.declaredType.items} == {
        "fixed_start": FIXED_START,
        "steady_state": STEADY_STATE,
    }


def test_invalid_init_mode_is_refused(new_fmu, vr) -> None:
    """A value that is no item of InitMode cannot be set at all."""
    log = []
    fmu = new_fmu(log=log)

    with pytest.raises(FMICallException):
        fmu.setInteger([vr("init_mode")], [3])

    assert any(
        "Value 3 of variable 'init_mode' is not an item of the enumeration 'InitMode'"
        in message
        for _, _, message in log
    ), log
    assert fmu.getInteger([vr("init_mode")])[0] == FIXED_START


def test_temperature_cannot_be_set_in_initialization_mode(new_fmu, vr) -> None:
    """T has initial="approx", so it can only be set before the initialization."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    with pytest.raises(FMICallException):
        fmu.setReal([vr("T")], [300.0])


def test_tau_cannot_be_set(new_fmu, vr) -> None:
    """tau is computed from R and C."""
    with pytest.raises(FMICallException):
        new_fmu().setReal([vr("tau")], [50.0])
