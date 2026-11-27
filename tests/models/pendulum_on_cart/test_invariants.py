"""Physical invariants of the pendulum_on_cart model.

Without a force on the cart the system is conservative (no friction), so the
energy and the horizontal momentum are constant:

    E = 1/2 (M+m) x_dot^2 + m l cos(theta) x_dot theta_dot + 1/2 m l^2 theta_dot^2 - m g l cos(theta)
    p = (M+m) x_dot + m l cos(theta) theta_dot

(theta measured from hanging down). RK4 does not conserve them exactly, but
their drift is an integration error and must vanish with order 4.

The pendulum starts with a large swing (theta = 2.5 rad, far from the linear
range) and a short length l = 0.1 m. The faster dynamics keep the drift above
rounding for every step size of the sweep; with the default length the finest
step would already be dominated by rounding. Every communication step is a
single integration step, because it is not larger than maxStepSize (1e-3).
"""

import math

import numpy as np

from tests.fmu_helpers import initialize

STOP_TIME = 10.0
STEP_SIZES = [1e-3, 5e-4, 2.5e-4]
STATES = ["state_x_dot", "state_theta", "state_theta_dot"]


def invariants(p: dict, x_dot: float, theta: float, theta_dot: float) -> tuple:
    """Energy and horizontal momentum of the pendulum on the cart."""
    M, m, l, g = p["M"], p["m"], p["l"], p["g"]
    cos = math.cos(theta)
    energy = (
        0.5 * (M + m) * x_dot**2
        + m * l * cos * x_dot * theta_dot
        + 0.5 * m * l**2 * theta_dot**2
        - m * g * l * cos
    )
    momentum = (M + m) * x_dot + m * l * cos * theta_dot
    return energy, momentum


def largest_drift(new_fmu, vr, step_size: float) -> tuple[float, float]:
    """Largest |E - E0| and |p - p0| over STOP_TIME, sampled at every communication point."""
    fmu = new_fmu(f"h_{step_size}")
    fmu.setReal([vr("l"), vr("state_theta"), vr("F")], [0.1, 2.5, 0.0])
    initialize(fmu)

    names = ["M", "m", "l", "g"]
    p = dict(zip(names, fmu.getReal([vr(name) for name in names])))
    references = [vr(name) for name in STATES]
    e0, p0 = invariants(p, *fmu.getReal(references))

    drift_e = drift_p = 0.0
    for k in range(round(STOP_TIME / step_size)):
        fmu.doStep(
            currentCommunicationPoint=k * step_size, communicationStepSize=step_size
        )
        energy, momentum = invariants(p, *fmu.getReal(references))
        drift_e = max(drift_e, abs(energy - e0))
        drift_p = max(drift_p, abs(momentum - p0))
    return drift_e, drift_p


def order(step_sizes, drifts) -> float:
    """Least-squares slope of log(drift) over log(step size)."""
    slope, _ = np.polyfit(np.log(step_sizes), np.log(drifts), 1)
    return slope


def test_energy_and_momentum_drift_with_fourth_order(new_fmu, vr) -> None:
    drifts = [largest_drift(new_fmu, vr, h) for h in STEP_SIZES]
    energy = [d[0] for d in drifts]
    momentum = [d[1] for d in drifts]

    # small: about 4e-11 (energy) and 1e-10 (momentum) at h = 1e-3
    assert energy[0] < 1e-9, f"energy drift {energy}"
    assert momentum[0] < 1e-9, f"momentum drift {momentum}"

    # an integration error, not a modelling error: it vanishes with order 4
    assert abs(order(STEP_SIZES, energy) - 4.0) <= 0.3, f"energy drift {energy}"
    assert abs(order(STEP_SIZES, momentum) - 4.0) <= 0.3, f"momentum drift {momentum}"
