import pytest
import fmpy
from pathlib import Path

def test_fmu_complete_lifecycle(path_to_project_root : Path, path_to_fmu : Path) -> None:
    """Test complete FMU lifecycle: Instantiate → Setup → Init → Step → Terminate → Free"""
    # Read model description
    model_description = fmpy.read_model_description(path_to_fmu)

    # Extract FMU
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))

    # Create FMU instance
    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='instance1'
    )

    # Instantiate
    fmu.instantiate()

    # Setup experiment
    fmu.setupExperiment(startTime=0.0)

    # Initialize
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    # Perform simulation step
    fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)

    # Terminate and free (explicit cleanup)
    fmu.terminate()
    fmu.freeInstance()
