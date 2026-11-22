"""Tests of the mass_spring_damper model against its analytic solution.

    m * x'' + d * x' + k * x = u

With m = k = d = 1 (the start values) the system is underdamped with
damping ratio 1/2 and damped frequency wd = sqrt(3)/2. For a unit step of u
at t = 0 and x(0) = v(0) = 0:

    x(t) = 1 - exp(-t/2) * (cos(wd*t) + sin(wd*t) / (2*wd))
    v(t) = exp(-t/2) * sin(wd*t) / wd
"""

import math

import pytest

from test.fmu_helpers import advance

WD = math.sqrt(3.0) / 2.0
TOLERANCE = 1e-12  # RK4 with sub-steps of 1e-3 stays at about 1e-14


def exact(t: float) -> tuple[float, float]:
    """Return (x, v) of the unit step response."""
    decay = math.exp(-t / 2.0)
    x = 1.0 - decay * (math.cos(WD * t) + math.sin(WD * t) / (2.0 * WD))
    v = decay * math.sin(WD * t) / WD
    return x, v


def test_parameters_give_the_tested_damping(initialized_fmu, vr) -> None:
    """The analytic solution below holds for m = k = d = 1."""
    assert initialized_fmu.getReal([vr("m"), vr("k"), vr("d")]) == [1.0, 1.0, 1.0]


def test_step_response_follows_the_analytic_solution(initialized_fmu, vr) -> None:
    fmu = initialized_fmu
    fmu.setReal([vr("u")], [1.0])

    step_size = 0.05
    for n in range(1, 101):
        advance(fmu, step_size, step_size, start=(n - 1) * step_size)
        t = n * step_size
        x, v = fmu.getReal([vr("x"), vr("v")])
        x_exact, v_exact = exact(t)
        assert x == pytest.approx(x_exact, abs=TOLERANCE), f"x at t = {t}"
        assert v == pytest.approx(v_exact, abs=TOLERANCE), f"v at t = {t}"


def test_outputs_equal_the_states(initialized_fmu, vr) -> None:
    """The outputs x and v publish the states state_x and state_v."""
    fmu = initialized_fmu
    fmu.setReal([vr("u")], [1.0])
    advance(fmu, 1.0)

    assert fmu.getReal([vr("x"), vr("v")]) == fmu.getReal(
        [vr("state_x"), vr("state_v")]
    )
