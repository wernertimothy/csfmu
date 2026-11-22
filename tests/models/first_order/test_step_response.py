"""Tests of the first_order model against its analytic solution.

    tau * dx/dt = -x + k*u,  y = x

For a step of u at t = 0 and x(0) = 0 the solution is y(t) = k*u*(1 - exp(-t/tau)).
The model is integrated with explicit Euler, so the error is of first order
in the step size.
"""

import math

import numpy as np

from tests.fmu_helpers import advance, initialize


def parameters(fmu, vr) -> tuple[float, float]:
    """Return (tau, k)."""
    tau, k = fmu.getReal([vr("tau"), vr("k")])
    return tau, k


def exact_y(t: float, tau: float, k: float, u: float) -> float:
    return k * u * (1.0 - math.exp(-t / tau))


def simulate_error(new_fmu, vr, step_size: float, stop_time: float) -> float:
    """Simulate a unit step from 0 to stop_time and return |y - y_exact|."""
    fmu = initialize(new_fmu(f"h_{step_size}"))
    tau, k = parameters(fmu, vr)
    fmu.setReal([vr("u")], [1.0])

    end = advance(fmu, stop_time, step_size)

    return abs(fmu.getReal([vr("y")])[0] - exact_y(end, tau, k, 1.0))


def test_initial_output_reflects_initial_state(new_fmu, vr) -> None:
    """The output is computed from the initial state at the end of initialization."""
    fmu = new_fmu()
    fmu.setReal([vr("x")], [0.7])
    initialize(fmu)

    assert abs(fmu.getReal([vr("y")])[0] - 0.7) < 1e-12


def test_step_response_follows_the_analytic_solution(initialized_fmu, vr) -> None:
    """Euler with sub-steps of 1e-3 stays within 2e-3 of the exact step response."""
    fmu = initialized_fmu
    tau, k = parameters(fmu, vr)
    fmu.setReal([vr("u")], [1.0])

    step_size = 0.01
    for n in range(1, 101):
        advance(fmu, step_size, step_size, start=(n - 1) * step_size)
        t = n * step_size
        y = fmu.getReal([vr("y")])[0]
        assert abs(y - exact_y(t, tau, k, 1.0)) < 2e-3, f"y at t = {t}"


def test_euler_converges_with_first_order(new_fmu, vr) -> None:
    """Halving the step size halves the error.

    Communication steps below maxStepSize (1e-3) are a single Euler sub-step,
    so the communication step size is the integration step size.
    """
    step_sizes = [1e-3, 5e-4, 2.5e-4, 1.25e-4]
    errors = [simulate_error(new_fmu, vr, h, stop_time=0.3) for h in step_sizes]

    order, _ = np.polyfit(np.log(step_sizes), np.log(errors), 1)
    assert abs(order - 1.0) <= 0.1, f"order {order:.3f}, errors {errors}"
