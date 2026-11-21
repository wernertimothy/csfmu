import pytest
from fmpy.fmi1 import FMICallException


@pytest.fixture(autouse=True)
def only_for_typed_demo(model_description) -> None:
    if model_description.modelName != "typed_demo":
        pytest.skip(f"Test specific to typed_demo model, got {model_description.modelName}")


def step(fmu, duration: float, step_size: float = 0.01, start_time: float = 0.0) -> float:
    """Advance the FMU by duration and return the new time."""
    time = start_time
    for _ in range(round(duration / step_size)):
        fmu.doStep(currentCommunicationPoint=time, communicationStepSize=step_size)
        time += step_size
    return time


def test_variable_types_in_model_description(model_description) -> None:
    """Each variable is declared with the type of its C struct field."""
    types = {v.name: v.type for v in model_description.modelVariables}

    assert types == {
        "rate": "Real", "direction": "Integer", "clamp": "Boolean", "limit": "Real",
        "enable": "Boolean", "boost": "Integer",
        "y": "Real", "level": "Integer", "limit_reached": "Boolean",
        "x": "Real",
    }


def test_integer_and_boolean_inputs_are_discrete(model_description) -> None:
    """FMI 2.0 only allows variability continuous for Real variables."""
    variability = {v.name: v.variability for v in model_description.modelVariables}

    assert variability["enable"] == "discrete"
    assert variability["boost"] == "discrete"
    assert variability["level"] == "discrete"
    assert variability["limit_reached"] == "discrete"
    assert variability["y"] == "continuous"
    assert variability["limit"] == "fixed"
    assert variability["rate"] == "tunable"


def test_start_values_of_all_types(initialized_fmu, vr) -> None:
    """Start values of Real, Integer and Boolean variables are applied."""
    assert initialized_fmu.getReal([vr("rate"), vr("limit")]) == [1.0, 5.0]
    assert initialized_fmu.getInteger([vr("direction"), vr("boost")]) == [1, 1]
    assert initialized_fmu.getBoolean([vr("enable"), vr("clamp")]) == [True, False]


def test_integer_round_trip(new_fmu, vr) -> None:
    fmu = new_fmu()
    fmu.setInteger([vr("direction"), vr("boost")], [-1, 7])
    assert fmu.getInteger([vr("boost"), vr("direction")]) == [7, -1]


def test_boolean_round_trip(new_fmu, vr) -> None:
    fmu = new_fmu()
    fmu.setBoolean([vr("enable"), vr("clamp")], [False, True])
    assert fmu.getBoolean([vr("clamp"), vr("enable")]) == [True, False]


def test_boolean_variable_cannot_be_accessed_as_real_or_integer(new_fmu, vr) -> None:
    with pytest.raises(FMICallException):
        new_fmu().getReal([vr("enable")])
    with pytest.raises(FMICallException):
        new_fmu().getInteger([vr("enable")])
    with pytest.raises(FMICallException):
        new_fmu().setReal([vr("enable")], [1.0])


def test_integer_variable_cannot_be_accessed_as_real_or_boolean(new_fmu, vr) -> None:
    with pytest.raises(FMICallException):
        new_fmu().getReal([vr("boost")])
    with pytest.raises(FMICallException):
        new_fmu().getBoolean([vr("boost")])


def test_ramp_runs_while_enabled(initialized_fmu, vr) -> None:
    """dx/dt = rate * direction * boost = 1 with the start values."""
    step(initialized_fmu, 2.5)

    assert initialized_fmu.getReal([vr("y")])[0] == pytest.approx(2.5, abs=1e-9)
    assert initialized_fmu.getInteger([vr("level")]) == [2]
    assert initialized_fmu.getBoolean([vr("limit_reached")]) == [False]


def test_boolean_input_stops_the_ramp(initialized_fmu, vr) -> None:
    time = step(initialized_fmu, 1.0)
    initialized_fmu.setBoolean([vr("enable")], [False])
    step(initialized_fmu, 1.0, start_time=time)

    assert initialized_fmu.getReal([vr("y")])[0] == pytest.approx(1.0, abs=1e-9)


def test_integer_input_and_parameter_scale_the_ramp(new_fmu, vr) -> None:
    fmu = new_fmu()
    fmu.setInteger([vr("direction")], [-1])
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    fmu.setInteger([vr("boost")], [3])

    step(fmu, 1.5)

    # y = -4.5, away from an integer so that rounding errors cannot change floor(y)
    assert fmu.getReal([vr("y")])[0] == pytest.approx(-4.5, abs=1e-9)
    assert fmu.getInteger([vr("level")]) == [-5]


def test_boolean_output_and_boolean_parameter(new_fmu, vr) -> None:
    """With clamp the output stops at the limit and limit_reached becomes true."""
    fmu = new_fmu()
    fmu.setBoolean([vr("clamp")], [True])
    fmu.setReal([vr("limit")], [2.0])
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    step(fmu, 3.0)

    assert fmu.getBoolean([vr("limit_reached")]) == [True]
    assert fmu.getReal([vr("y")])[0] == pytest.approx(2.0, abs=1e-9)
    assert fmu.getReal([vr("x")])[0] == pytest.approx(3.0, abs=1e-9)


def test_fixed_parameter_cannot_be_set_after_initialization(initialized_fmu, vr) -> None:
    with pytest.raises(FMICallException):
        initialized_fmu.setReal([vr("limit")], [1.0])


def test_tunable_parameter_can_be_set_after_initialization(initialized_fmu, vr) -> None:
    initialized_fmu.setReal([vr("rate")], [2.0])
    step(initialized_fmu, 1.0)

    assert initialized_fmu.getReal([vr("y")])[0] == pytest.approx(2.0, abs=1e-9)
