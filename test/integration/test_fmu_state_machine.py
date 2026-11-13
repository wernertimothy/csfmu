import pytest
import fmpy

"""
Model-independent tests of the FMU state machine.

A call that is not allowed in the current state returns fmi2Error. From then
on the FMU is in the error state: it can only be reset or freed (reading
variables stays possible).
"""


def test_cannot_do_step_before_exiting_initialization_mode(new_fmu) -> None:
    """fmi2DoStep is not allowed in initialization mode."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    with pytest.raises(Exception):
        fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)


def test_cannot_enter_initialization_mode_after_exiting(initialized_fmu) -> None:
    """Initialization mode can only be entered from the instantiated state."""
    with pytest.raises(Exception):
        initialized_fmu.enterInitializationMode()


def test_cannot_setup_experiment_after_entering_initialization_mode(new_fmu) -> None:
    """fmi2SetupExperiment must be called before fmi2EnterInitializationMode."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    with pytest.raises(Exception):
        fmu.setupExperiment(startTime=1.0)


def test_cannot_do_step_after_terminate(initialized_fmu) -> None:
    """After fmi2Terminate the FMU can only be reset or freed."""
    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)
    initialized_fmu.terminate()

    with pytest.raises(Exception):
        initialized_fmu.doStep(currentCommunicationPoint=0.01, communicationStepSize=0.01)


def test_error_state_is_kept_until_reset(initialized_fmu, model_description) -> None:
    """After an error every call fails until the FMU is reset."""
    variable = model_description.modelVariables[0]
    getter = {"Real": "getReal", "Integer": "getInteger", "Boolean": "getBoolean"}[variable.type]

    # cause an error: wrong call sequence
    with pytest.raises(Exception):
        initialized_fmu.enterInitializationMode()

    # calls that would have been fine before now fail
    with pytest.raises(Exception):
        initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)
    with pytest.raises(Exception):
        initialized_fmu.terminate()

    # variables can still be read for diagnosis
    getattr(initialized_fmu, getter)([variable.valueReference])

    # reset is the way out
    initialized_fmu.reset()
    initialized_fmu.setupExperiment(startTime=0.0)
    initialized_fmu.enterInitializationMode()
    initialized_fmu.exitInitializationMode()
    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)


def test_cannot_step_beyond_stop_time(new_fmu) -> None:
    """If a stop time is defined, a step that ends behind it is an error."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0, stopTime=0.1)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.1)

    with pytest.raises(Exception):
        fmu.doStep(currentCommunicationPoint=0.1, communicationStepSize=0.1)


def test_can_step_exactly_to_stop_time(new_fmu) -> None:
    """Rounding errors of the master clock do not trigger the stop time check."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0, stopTime=1.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    time = 0.0
    for _ in range(10):
        fmu.doStep(currentCommunicationPoint=time, communicationStepSize=0.1)
        time += 0.1  # ends at 0.9999999999999999


def test_step_must_start_at_the_internal_clock(initialized_fmu) -> None:
    """currentCommunicationPoint must be the end of the previous step."""
    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.1)

    with pytest.raises(Exception):
        initialized_fmu.doStep(currentCommunicationPoint=0.5, communicationStepSize=0.1)


@pytest.mark.parametrize("step_size", [0.0, -0.1, float("nan"), float("inf")])
def test_step_size_must_be_positive_and_finite(initialized_fmu, step_size) -> None:
    with pytest.raises(Exception):
        initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=step_size)


def test_clock_does_not_drift_at_large_times(new_fmu) -> None:
    """The clock check uses a relative tolerance and follows the master clock."""
    start = 1.0e7
    fmu = new_fmu()
    fmu.setupExperiment(startTime=start)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    time = start
    for _ in range(200):
        fmu.doStep(currentCommunicationPoint=time, communicationStepSize=0.001)
        time += 0.001


def test_model_exchange_is_rejected(path_to_project_root, path_to_fmu, model_description) -> None:
    """The FMU is Co-Simulation only: instantiating it as Model Exchange fails."""
    unzipdir = fmpy.extract(path_to_fmu, path_to_project_root.joinpath("work"))
    fmu = fmpy.fmi2.FMU2Model(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName="me_instance",
    )

    with pytest.raises(Exception):
        fmu.instantiate()


def test_unsupported_functions_fail(initialized_fmu, model_description) -> None:
    """Functions of capabilities the FMU does not have return fmi2Error."""
    assert not model_description.coSimulation.canInterpolateInputs

    with pytest.raises(Exception):
        initialized_fmu.cancelStep()


def test_unknown_log_category_is_ignored(new_fmu) -> None:
    """An unknown log category only causes a warning; the FMU stays usable."""
    fmu = new_fmu()
    fmu.setDebugLogging(loggingOn=True, categories=["logEvents", "noSuchCategory"])
    fmu.setupExperiment(startTime=0.0)


def test_status_queries(initialized_fmu) -> None:
    """Status of a synchronous slave: last step finished, clock, not terminated."""
    fmi2DoStepStatus, fmi2PendingStatus, fmi2LastSuccessfulTime, fmi2Terminated = range(4)

    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.25)

    assert initialized_fmu.getStatus(fmi2DoStepStatus) == 0  # fmi2OK
    assert initialized_fmu.getRealStatus(fmi2LastSuccessfulTime) == 0.25
    assert initialized_fmu.getBooleanStatus(fmi2Terminated) == 0
    assert initialized_fmu.getStringStatus(fmi2PendingStatus) in (b"", "")

    # a status kind that is not available is answered with fmi2Discard,
    # which does not put the FMU into the error state
    with pytest.raises(Exception):
        initialized_fmu.getIntegerStatus(fmi2DoStepStatus)
    with pytest.raises(Exception):
        initialized_fmu.getRealStatus(fmi2Terminated)

    initialized_fmu.doStep(currentCommunicationPoint=0.25, communicationStepSize=0.25)
