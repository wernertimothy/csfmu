import pytest
import fmpy
from pathlib import Path


def test_cannot_do_step_before_exiting_initialization_mode(path_to_project_root: Path, path_to_fmu: Path) -> None:
    """
    Verify FMU rejects doStep when called in initialization mode.

    State machine violation: doStep requires state to be StepComplete or Initialized,
    but should not be callable in InitializationMode.
    """
    model_description = fmpy.read_model_description(path_to_fmu)
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))

    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='test_instance'
    )

    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    # Should fail: trying to step while in initialization mode
    with pytest.raises(Exception):
        fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)

    # Cleanup
    fmu.exitInitializationMode()
    fmu.terminate()
    fmu.freeInstance()


def test_cannot_enter_initialization_mode_after_exiting(path_to_project_root: Path, path_to_fmu: Path) -> None:
    """
    Verify FMU rejects enterInitializationMode when called after exitInitializationMode.

    State machine violation: Can only enter initialization mode from Instantiated state,
    not from StepComplete/Initialized states.
    """
    model_description = fmpy.read_model_description(path_to_fmu)
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))

    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='test_instance'
    )

    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    # Should fail: cannot re-enter initialization mode
    with pytest.raises(Exception):
        fmu.enterInitializationMode()

    # Cleanup
    fmu.terminate()
    fmu.freeInstance()


def test_cannot_setup_experiment_after_entering_initialization_mode(path_to_project_root: Path, path_to_fmu: Path) -> None:
    """
    Verify FMU rejects setupExperiment when called after enterInitializationMode.

    State machine violation: setupExperiment must be called in Instantiated state,
    not after entering InitializationMode.
    """
    model_description = fmpy.read_model_description(path_to_fmu)
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))

    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='test_instance'
    )

    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    # Should fail: cannot call setupExperiment after entering initialization mode
    with pytest.raises(Exception):
        fmu.setupExperiment(startTime=1.0)

    # Cleanup
    fmu.exitInitializationMode()
    fmu.terminate()
    fmu.freeInstance()


def test_cannot_do_step_after_terminate(path_to_project_root: Path, path_to_fmu: Path) -> None:
    """
    Verify FMU rejects doStep when called after terminate.

    State machine violation: doStep cannot be called in Terminated state,
    only FreeInstance or Reset are allowed.
    """
    model_description = fmpy.read_model_description(path_to_fmu)
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))

    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='test_instance'
    )

    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)
    fmu.terminate()

    # Should fail: cannot step after termination
    with pytest.raises(Exception):
        fmu.doStep(currentCommunicationPoint=0.01, communicationStepSize=0.01)

    # Cleanup
    fmu.freeInstance()
