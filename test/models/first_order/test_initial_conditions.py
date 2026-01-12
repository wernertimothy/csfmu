import pytest
import fmpy
from pathlib import Path


def test_initial_parameters(path_to_project_root: Path, path_to_fmu: Path) -> None:
    """
    Verify initial parameter values match modelDescription.xml for first_order model.

    Expected:
        - tau (VR=0): 0.15 (time constant)
        - k (VR=1): 1.0 (static gain)
    """
    # Read model description
    model_description = fmpy.read_model_description(path_to_fmu)

    # Skip if not first_order model
    if model_description.modelName != "first_order":
        pytest.skip(f"Test specific to first_order model, got {model_description.modelName}")

    # Extract FMU
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))

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

    # Get parameter values (tau=VR0, k=VR1)
    params = fmu.getReal([0, 1])
    tau = params[0]
    k = params[1]

    # Verify initial values
    assert abs(tau - 0.15) < 1e-9, f"Expected tau = 0.15, got {tau}"
    assert abs(k - 1.0) < 1e-9, f"Expected k = 1.0, got {k}"

    # Cleanup
    fmu.terminate()
    fmu.freeInstance()


def test_initial_inputs(path_to_project_root: Path, path_to_fmu: Path) -> None:
    """
    Verify initial input values match modelDescription.xml for first_order model.

    Expected:
        - u (VR=2): 0.0 (input signal)
    """
    # Read model description
    model_description = fmpy.read_model_description(path_to_fmu)

    # Skip if not first_order model
    if model_description.modelName != "first_order":
        pytest.skip(f"Test specific to first_order model, got {model_description.modelName}")

    # Extract FMU
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))

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

    # Get input value (u=VR2)
    u_initial = fmu.getReal([2])[0]

    # Verify initial value
    assert abs(u_initial - 0.0) < 1e-9, f"Expected u = 0.0, got {u_initial}"

    # Cleanup
    fmu.terminate()
    fmu.freeInstance()


def test_initial_outputs(path_to_project_root: Path, path_to_fmu: Path) -> None:
    """
    Verify initial output values for first_order model.

    Expected:
        - y (VR=3): 0.0 (output equals state x, which starts at 0)
    """
    # Read model description
    model_description = fmpy.read_model_description(path_to_fmu)

    # Skip if not first_order model
    if model_description.modelName != "first_order":
        pytest.skip(f"Test specific to first_order model, got {model_description.modelName}")

    # Extract FMU
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))

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

    # Get output value (y=VR3)
    y_initial = fmu.getReal([3])[0]

    # Verify initial value: y(0) = x(0) = 0
    assert abs(y_initial - 0.0) < 1e-9, f"Expected y = 0.0, got {y_initial}"

    # Cleanup
    fmu.terminate()
    fmu.freeInstance()
