import math

import numpy as np
import pytest

"""
Tests of time in the model layer (ROADMAP P0.1) with the forced_pt1 model:

    tau * dx/dt = -x + k*u + a*sin(omega*t),  y = x,  excitation = a*sin(omega*t)

With u = 0 and x(0) = 0 the solution is

    x(t) = a / (1 + (omega*tau)^2) * [sin(omega*t) - omega*tau*cos(omega*t) + omega*tau*exp(-t/tau)]
"""


@pytest.fixture(autouse=True)
def only_forced_pt1(model_description) -> None:
    """Skip these tests if the FMU under test is not the forced_pt1 model."""
    if model_description.modelName != "forced_pt1":
        pytest.skip(
            f"Test specific to forced_pt1 model, got {model_description.modelName}"
        )


def parameters(fmu, vr) -> dict:
    """Read the parameters of the model."""
    names = ["tau", "k", "a", "omega"]
    return dict(zip(names, fmu.getReal([vr(name) for name in names])))


def exact_x(t: float, tau: float, a: float, omega: float) -> float:
    """Analytic solution for u = 0 and x(0) = 0."""
    wt = omega * tau
    return (
        a
        / (1.0 + wt**2)
        * (math.sin(omega * t) - wt * math.cos(omega * t) + wt * math.exp(-t / tau))
    )


def simulate_error(new_fmu, vr, step_size: float, stop_time: float) -> float:
    """Simulate from 0 to stop_time with a fixed communication step and return |y - x_exact|."""
    fmu = new_fmu(f"h_{step_size}")
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    p = parameters(fmu, vr)

    steps = round(stop_time / step_size)
    for k in range(steps):
        fmu.doStep(
            currentCommunicationPoint=k * step_size, communicationStepSize=step_size
        )

    y = fmu.getReal([vr("y")])[0]
    return abs(y - exact_x(steps * step_size, p["tau"], p["a"], p["omega"]))


def convergence_order(step_sizes, errors) -> float:
    """Least-squares slope of log(error) over log(step size)."""
    slope, _ = np.polyfit(np.log(step_sizes), np.log(errors), 1)
    return slope


def test_outputs_are_evaluated_at_the_communication_point(initialized_fmu, vr) -> None:
    """The time-only output equals a*sin(omega*t) at every communication point."""
    p = parameters(initialized_fmu, vr)
    step_size = 0.01

    assert initialized_fmu.getReal([vr("excitation")])[0] == 0.0
    for k in range(50):
        initialized_fmu.doStep(
            currentCommunicationPoint=k * step_size, communicationStepSize=step_size
        )
        t = (k + 1) * step_size
        excitation = initialized_fmu.getReal([vr("excitation")])[0]
        assert abs(excitation - p["a"] * math.sin(p["omega"] * t)) < 1e-12, (
            f"at t = {t}"
        )


def test_start_time_is_honored(new_fmu, vr) -> None:
    """fmi2SetupExperiment sets the model time, fmi2Reset sets it back to 0."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.25)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    # a = 1, omega = 2*pi: sin(pi/2) = 1
    assert abs(fmu.getReal([vr("excitation")])[0] - 1.0) < 1e-12
    fmu.doStep(currentCommunicationPoint=0.25, communicationStepSize=0.25)
    assert abs(fmu.getReal([vr("excitation")])[0] - 0.0) < 1e-12

    fmu.reset()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    assert fmu.getReal([vr("excitation")])[0] == 0.0


def test_trajectory_matches_the_analytic_solution(new_fmu, vr) -> None:
    """RK4 with h = 0.01 follows the analytic solution closely."""
    assert simulate_error(new_fmu, vr, step_size=0.01, stop_time=1.0) < 1e-9


def test_rk4_keeps_fourth_order_on_a_time_varying_model(new_fmu, vr) -> None:
    """RK4 converges with order 4 only if the stages are evaluated at their own times.

    Each communication step is a single sub-step (maxStepSize = 0.1). With the
    stage times frozen at the step start the slope drops to about 0.8.
    """
    step_sizes = [0.1, 0.05, 0.025, 0.0125]
    errors = [simulate_error(new_fmu, vr, h, stop_time=1.0) for h in step_sizes]

    order = convergence_order(step_sizes, errors)
    assert abs(order - 4.0) <= 0.3, f"order {order:.3f}, errors {errors}"
