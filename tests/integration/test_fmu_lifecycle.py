"""Model-independent test of the complete life of an FMU."""

from fmpy.fmi2 import fmi2LastSuccessfulTime

from tests.fmu_helpers import advance


def test_fmu_complete_lifecycle(new_fmu) -> None:
    """
    Run the FMU through every phase of its life without value references.

    Instantiate → SetupExperiment → EnterInit → ExitInit → DoStep (multi) → Terminate → Free
    """
    fmu = new_fmu("instance1")

    # inquiry functions
    assert fmu.getVersion() == "2.0"
    assert fmu.getTypesPlatform() == "default"

    # debug logging control
    fmu.setDebugLogging(loggingOn=True, categories=[])  # all categories
    fmu.setDebugLogging(loggingOn=False, categories=[])

    fmu.setupExperiment(startTime=0.0, stopTime=1.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    # 100 steps up to the stop time; the FMU's clock follows the master
    end = advance(fmu, 1.0, step_size=0.01)
    assert abs(fmu.getRealStatus(fmi2LastSuccessfulTime) - end) < 1e-9

    fmu.terminate()
    # fmi2FreeInstance is called by the new_fmu fixture
