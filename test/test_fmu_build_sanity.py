import pytest
import fmpy
from pathlib import Path

def test_fmu_model_description_loads(path_to_fmu : Path) -> None:
    model_description = fmpy.read_model_description(path_to_fmu)
    assert True

def test_fmu_does_simualte(path_to_project_root : Path, path_to_fmu : Path) -> None:
    model_description = fmpy.read_model_description(path_to_fmu)
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))
    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='instance1'
    )
    fmu.instantiate()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)
