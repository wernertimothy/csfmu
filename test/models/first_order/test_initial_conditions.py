from pathlib import Path

import fmpy
import pytest


def test_initial_parameters(unzipdir: Path, path_to_fmu: Path, vr) -> None:
    """
    Verify initial parameter values match modelDescription.xml for first_order model.

    Expected:
        - tau: 0.15 (time constant)
        - k: 1.0 (static gain)
    """
    # Read model description
    model_description = fmpy.read_model_description(path_to_fmu)

    # Skip if not first_order model
    if model_description.modelName != "first_order":
        pytest.skip(f"Test specific to first_order model, got {model_description.modelName}")

    # Create FMU instance
    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='param_test'
    )

    # Setup and initialize
    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    # Get parameter values
    params = fmu.getReal([vr("tau"), vr("k")])
    tau = params[0]
    k = params[1]

    # Verify initial values
    assert abs(tau - 0.15) < 1e-9, f"Expected tau = 0.15, got {tau}"
    assert abs(k - 1.0) < 1e-9, f"Expected k = 1.0, got {k}"

    # Cleanup
    fmu.terminate()
    fmu.freeInstance()


def test_initial_inputs(unzipdir: Path, path_to_fmu: Path, vr) -> None:
    """
    Verify initial input values match modelDescription.xml for first_order model.

    Expected:
        - u: 0.0 (input signal)
    """
    # Read model description
    model_description = fmpy.read_model_description(path_to_fmu)

    # Skip if not first_order model
    if model_description.modelName != "first_order":
        pytest.skip(f"Test specific to first_order model, got {model_description.modelName}")

    # Create FMU instance
    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='input_test'
    )

    # Setup and initialize
    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    # Get input value
    u_initial = fmu.getReal([vr("u")])[0]

    # Verify initial value
    assert abs(u_initial - 0.0) < 1e-9, f"Expected u = 0.0, got {u_initial}"

    # Cleanup
    fmu.terminate()
    fmu.freeInstance()


def test_initial_outputs(unzipdir: Path, path_to_fmu: Path, vr) -> None:
    """
    Verify initial output values for first_order model.

    Expected:
        - y: 0.0 (output equals state x, which starts at 0)
    """
    # Read model description
    model_description = fmpy.read_model_description(path_to_fmu)

    # Skip if not first_order model
    if model_description.modelName != "first_order":
        pytest.skip(f"Test specific to first_order model, got {model_description.modelName}")

    # Create FMU instance
    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='output_test'
    )

    # Setup and initialize
    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    # Get output value
    y_initial = fmu.getReal([vr("y")])[0]

    # Verify initial value: y(0) = x(0) = 0
    assert abs(y_initial - 0.0) < 1e-9, f"Expected y = 0.0, got {y_initial}"

    # Cleanup
    fmu.terminate()
    fmu.freeInstance()


def test_initial_output_reflects_initial_state(new_fmu, model_description, vr) -> None:
    """
    Verify the output is computed from the initial state at the end of initialization.

    The state x is set before initialization; y = x must hold at t = 0.
    """
    if model_description.modelName != "first_order":
        pytest.skip(f"Test specific to first_order model, got {model_description.modelName}")

    fmu = new_fmu()
    fmu.setReal([vr("x")], [0.7])
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    y_initial = fmu.getReal([vr("y")])[0]

    assert abs(y_initial - 0.7) < 1e-12, f"Expected y = 0.7, got {y_initial}"
