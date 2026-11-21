from pathlib import Path

import fmpy
import pytest


def test_initial_parameters(unzipdir: Path, path_to_fmu: Path, vr) -> None:
    """
    Verify initial parameter values match modelDescription.xml for mass_spring_damper model.

    Expected:
        - m: 1.0 (mass)
        - k: 1.0 (spring stiffness)
        - d: 1.0 (damping coefficient)
    """
    model_description = fmpy.read_model_description(path_to_fmu)

    if model_description.modelName != "mass_spring_damper":
        pytest.skip(f"Test specific to mass_spring_damper model, got {model_description.modelName}")

    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='param_test'
    )

    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    params = fmu.getReal([vr("m"), vr("k"), vr("d")])

    assert abs(params[0] - 1.0) < 1e-9, f"Expected m = 1.0, got {params[0]}"
    assert abs(params[1] - 1.0) < 1e-9, f"Expected k = 1.0, got {params[1]}"
    assert abs(params[2] - 1.0) < 1e-9, f"Expected d = 1.0, got {params[2]}"

    fmu.terminate()
    fmu.freeInstance()


def test_initial_inputs(unzipdir: Path, path_to_fmu: Path, vr) -> None:
    """
    Verify initial input values match modelDescription.xml for mass_spring_damper model.

    Expected:
        - u: 0.0 (external force)
    """
    model_description = fmpy.read_model_description(path_to_fmu)

    if model_description.modelName != "mass_spring_damper":
        pytest.skip(f"Test specific to mass_spring_damper model, got {model_description.modelName}")

    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='input_test'
    )

    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    u_initial = fmu.getReal([vr("u")])[0]

    assert abs(u_initial - 0.0) < 1e-9, f"Expected u = 0.0, got {u_initial}"

    fmu.terminate()
    fmu.freeInstance()


def test_initial_outputs(unzipdir: Path, path_to_fmu: Path, vr) -> None:
    """
    Verify initial output values for mass_spring_damper model.

    Expected:
        - x: 0.0 (position starts at equilibrium)
        - v: 0.0 (velocity starts at zero)
    """
    model_description = fmpy.read_model_description(path_to_fmu)

    if model_description.modelName != "mass_spring_damper":
        pytest.skip(f"Test specific to mass_spring_damper model, got {model_description.modelName}")

    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='output_test'
    )

    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    outputs = fmu.getReal([vr("x"), vr("v")])

    assert abs(outputs[0] - 0.0) < 1e-9, f"Expected x = 0.0, got {outputs[0]}"
    assert abs(outputs[1] - 0.0) < 1e-9, f"Expected v = 0.0, got {outputs[1]}"

    fmu.terminate()
    fmu.freeInstance()
