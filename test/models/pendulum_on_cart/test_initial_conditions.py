from pathlib import Path

import pytest
from fmpy import simulate_fmu


@pytest.fixture(autouse=True)
def only_for_pendulum_on_cart(model_description) -> None:
    if model_description.modelName != "pendulum_on_cart":
        pytest.skip(
            f"Test specific to pendulum_on_cart model, got {model_description.modelName}"
        )


def test_initial_outputs_reflect_initial_states(initialized_fmu, vr) -> None:
    """
    Outputs are valid right after initialization.

    The pendulum starts at an angle of 0.1 rad, so theta must read 0.1 at
    t = 0 (and not 0 until the first step).
    """
    x, theta = initialized_fmu.getReal([vr("x"), vr("theta")])

    assert abs(x - 0.0) < 1e-12, f"Expected x = 0.0, got {x}"
    assert abs(theta - 0.1) < 1e-12, f"Expected theta = 0.1, got {theta}"


def test_initial_angle_can_be_set_by_name(path_to_fmu: Path) -> None:
    """The initial condition of a state can be set through start_values."""
    result = simulate_fmu(
        str(path_to_fmu),
        start_values={"state_theta": 0.3},
        output=["theta"],
        stop_time=0.1,
        output_interval=0.01,
    )

    assert abs(result["theta"][0] - 0.3) < 1e-12, (
        f"Expected theta(0) = 0.3, got {result['theta'][0]}"
    )


def test_parameter_start_values_take_effect(path_to_fmu: Path) -> None:
    """
    Parameters passed as start_values change the simulation result.

    Without gravity the pendulum stays at its initial angle; with the
    default gravity it swings.
    """
    without_gravity = simulate_fmu(
        str(path_to_fmu),
        start_values={"g": 0.0},
        output=["theta"],
        stop_time=0.5,
        output_interval=0.01,
    )
    with_gravity = simulate_fmu(
        str(path_to_fmu), output=["theta"], stop_time=0.5, output_interval=0.01
    )

    assert abs(without_gravity["theta"][-1] - 0.1) < 1e-9, (
        "theta must stay at 0.1 without gravity"
    )
    assert abs(with_gravity["theta"][-1] - 0.1) > 1e-3, "theta must change with gravity"
