import pytest

"""
Tests of direct feedthrough (ROADMAP P0.4) with the pi_controller model:

    e = r - y_m,  dx_I/dt = e,  u = Kp*e + Ki*x_I

With constant inputs x_I grows linearly, x_I(t) = x_I(t0) + e*(t - t0), which
RK4 integrates exactly. The outputs u and e depend on the inputs directly, so
they must react to fmi2SetReal without a step.
"""

KP = 2.0
KI = 1.0
TOLERANCE = 1e-12


@pytest.fixture(autouse=True)
def only_pi_controller(model_description) -> None:
    """Skip these tests if the FMU under test is not the pi_controller model."""
    if model_description.modelName != "pi_controller":
        pytest.skip(
            f"Test specific to pi_controller model, got {model_description.modelName}"
        )


def outputs(fmu, vr) -> tuple[float, float]:
    """Return (u, e)."""
    u, e = fmu.getReal([vr("u"), vr("e")])
    return u, e


def test_output_reacts_to_the_reference_without_a_step(initialized_fmu, vr) -> None:
    assert outputs(initialized_fmu, vr) == (0.0, 0.0)

    initialized_fmu.setReal([vr("r")], [1.0])

    assert outputs(initialized_fmu, vr) == (KP * 1.0, 1.0)


def test_output_reacts_to_the_measurement_without_a_step(initialized_fmu, vr) -> None:
    initialized_fmu.setReal([vr("r")], [1.0])
    initialized_fmu.setReal([vr("y_m")], [0.25])

    assert outputs(initialized_fmu, vr) == (KP * 0.75, 0.75)


def test_feedthrough_in_initialization_mode(new_fmu, vr) -> None:
    """Outputs read in initialization mode already follow the inputs set there.

    The outputs are read once before the set. Otherwise they would still be
    stale from fmi2Instantiate and the check would not show that a set
    triggers their recomputation.
    """
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    assert outputs(fmu, vr) == (0.0, 0.0)

    fmu.setReal([vr("r")], [1.0])
    assert outputs(fmu, vr) == (KP * 1.0, 1.0)

    fmu.exitInitializationMode()
    assert outputs(fmu, vr) == (KP * 1.0, 1.0)


def test_integral_action_matches_the_analytic_solution(initialized_fmu, vr) -> None:
    """x_I = e*t for constant e; a new input acts on u at once and on x_I from then on."""
    fmu = initialized_fmu
    step_size = 0.1

    # e = 1 on [0, 1]: x_I = t, u = Kp + Ki*t
    fmu.setReal([vr("r")], [1.0])
    for k in range(10):
        fmu.doStep(
            currentCommunicationPoint=k * step_size, communicationStepSize=step_size
        )
        t = (k + 1) * step_size
        x_i = fmu.getReal([vr("x_I")])[0]
        u, _ = outputs(fmu, vr)
        assert abs(x_i - t) < TOLERANCE, f"x_I at t = {t}"
        assert abs(u - (KP + KI * t)) < TOLERANCE, f"u at t = {t}"

    # at t = 1, e drops to 0.5: u jumps at once, x_I is continuous
    fmu.setReal([vr("y_m")], [0.5])
    u, e = outputs(fmu, vr)
    assert e == 0.5
    assert abs(u - (KP * 0.5 + KI * 1.0)) < TOLERANCE

    # e = 0.5 on [1, 1.5]: x_I = 1 + 0.5*(t - 1)
    for k in range(10, 15):
        fmu.doStep(
            currentCommunicationPoint=k * step_size, communicationStepSize=step_size
        )
        t = (k + 1) * step_size
        x_i_exact = 1.0 + 0.5 * (t - 1.0)
        u, _ = outputs(fmu, vr)
        assert abs(fmu.getReal([vr("x_I")])[0] - x_i_exact) < TOLERANCE, (
            f"x_I at t = {t}"
        )
        assert abs(u - (KP * 0.5 + KI * x_i_exact)) < TOLERANCE, f"u at t = {t}"


def test_model_structure_declares_the_feedthrough(model_description) -> None:
    """Both outputs list exactly the two inputs as dependencies."""
    dependencies = {
        unknown.variable.name: {variable.name for variable in unknown.dependencies}
        for unknown in model_description.outputs
    }

    assert dependencies == {"u": {"r", "y_m"}, "e": {"r", "y_m"}}
