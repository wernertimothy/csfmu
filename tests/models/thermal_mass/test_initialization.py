"""Tests of the initialization of the thermal_mass model (ROADMAP P1.3).

    tau = R*C                              (calculated parameter)
    init_mode 1 (fixed_start):  T(0) = start value of T
    init_mode 2 (steady_state): T(0) = T_amb + R*P, so that dT/dt = 0

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


def test_invalid_init_mode_fails_the_initialization(new_fmu, vr) -> None:
    """An init_mode outside {1, 2} is reported by fmi2ExitInitializationMode."""
    log = []
    fmu = new_fmu(log=log)
    fmu.setInteger([vr("init_mode")], [3])
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    with pytest.raises(FMICallException):
        fmu.exitInitializationMode()

    assert any(
        "Initialization failed: init_mode 3 is neither" in message
        for _, _, message in log
    ), log


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
