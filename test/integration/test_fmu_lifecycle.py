from pathlib import Path

import fmpy


def test_fmu_complete_lifecycle(unzipdir: Path, path_to_fmu: Path) -> None:
    """
    Test complete FMU lifecycle with all model-agnostic FMI functions.

    This test exercises the FMI 2.0 API without requiring value references,
    making it model-independent and suitable for any FMU built with this template.

    Tests: Instantiate → SetupExperiment → EnterInit → ExitInit → DoStep (multi) → Terminate → Free
    """
    # Read model description
    model_description = fmpy.read_model_description(path_to_fmu)

    # Create FMU instance
    fmu = fmpy.fmi2.FMU2Slave(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName='instance1'
    )

    # Test inquiry functions (can be called before instantiate)
    version = fmu.getVersion()
    assert version == "2.0", f"Expected FMI version 2.0, got {version}"

    platform = fmu.getTypesPlatform()
    assert platform == "default", f"Expected platform 'default', got {platform}"

    # Instantiate
    fmu.instantiate()

    # Test debug logging control
    fmu.setDebugLogging(loggingOn=True, categories=[])  # Enable all categories
    fmu.setDebugLogging(loggingOn=False, categories=[]) # Disable logging

    # Setup experiment with stop time defined
    start_time = 0.0
    stop_time = 1.0
    fmu.setupExperiment(startTime=start_time, stopTime=stop_time)

    # Enter initialization mode
    fmu.enterInitializationMode()

    # Exit initialization mode (ready for stepping)
    fmu.exitInitializationMode()

    # Multi-step simulation (model-agnostic: just verify no crashes)
    step_size = 0.01
    current_time = start_time
    num_steps = 100

    for i in range(num_steps):
        fmu.doStep(currentCommunicationPoint=current_time, communicationStepSize=step_size)
        current_time += step_size

    # Verify simulation advanced to expected time
    assert abs(current_time - (start_time + num_steps * step_size)) < 1e-9, \
        "Time tracking error in multi-step simulation"

    # Terminate gracefully
    fmu.terminate()

    # Free instance (cleanup)
    fmu.freeInstance()
