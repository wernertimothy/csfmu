import fmpy
import pytest
from fmpy.fmi1 import FMICallException

"""
Model-independent tests of the FMU state machine.

A call that is not allowed in the current state returns fmi2Error. From then
on the FMU is in the error state: it can only be reset or freed (reading
variables stays possible).
"""

FMI2_OK = 0
FMI2_WARNING = 1


def do_step(fmu, time: float, step_size: float) -> int:
    """fmi2DoStep returning its status. FMU2Slave.doStep drops it and raises only
    for statuses above fmi2Warning, so the low-level call is used."""
    return fmu.fmi2DoStep(fmu.component, time, step_size, fmpy.fmi2.fmi2True)


def test_cannot_do_step_before_exiting_initialization_mode(new_fmu) -> None:
    """fmi2DoStep is not allowed in initialization mode."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    with pytest.raises(FMICallException):
        fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)


def test_cannot_enter_initialization_mode_after_exiting(initialized_fmu) -> None:
    """Initialization mode can only be entered from the instantiated state."""
    with pytest.raises(FMICallException):
        initialized_fmu.enterInitializationMode()


def test_cannot_setup_experiment_after_entering_initialization_mode(new_fmu) -> None:
    """fmi2SetupExperiment must be called before fmi2EnterInitializationMode."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()

    with pytest.raises(FMICallException):
        fmu.setupExperiment(startTime=1.0)


def test_cannot_do_step_after_terminate(initialized_fmu) -> None:
    """After fmi2Terminate the FMU can only be reset or freed."""
    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)
    initialized_fmu.terminate()

    with pytest.raises(FMICallException):
        initialized_fmu.doStep(
            currentCommunicationPoint=0.01, communicationStepSize=0.01
        )


def test_error_state_is_kept_until_reset(initialized_fmu, model_description) -> None:
    """After an error every call fails until the FMU is reset."""
    variable = model_description.modelVariables[0]
    getter = {"Real": "getReal", "Integer": "getInteger", "Boolean": "getBoolean"}[
        variable.type
    ]

    # cause an error: wrong call sequence
    with pytest.raises(FMICallException):
        initialized_fmu.enterInitializationMode()

    # calls that would have been fine before now fail
    with pytest.raises(FMICallException):
        initialized_fmu.doStep(
            currentCommunicationPoint=0.0, communicationStepSize=0.01
        )
    with pytest.raises(FMICallException):
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

    with pytest.raises(FMICallException):
        fmu.doStep(currentCommunicationPoint=0.1, communicationStepSize=0.1)


def test_can_step_exactly_to_stop_time(new_fmu) -> None:
    """Rounding errors of the master clock do not trigger the stop time check."""
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0, stopTime=1.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    time = 0.0
    for _ in range(10):
        assert do_step(fmu, time, 0.1) == FMI2_OK
        time += 0.1  # ends at 0.9999999999999999


def test_step_must_start_at_the_internal_clock(initialized_fmu) -> None:
    """currentCommunicationPoint must be the end of the previous step."""
    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.1)

    with pytest.raises(FMICallException):
        initialized_fmu.doStep(currentCommunicationPoint=0.5, communicationStepSize=0.1)


@pytest.mark.parametrize("step_size", [0.0, -0.1, float("nan"), float("inf")])
def test_step_size_must_be_positive_and_finite(initialized_fmu, step_size) -> None:
    with pytest.raises(FMICallException):
        initialized_fmu.doStep(
            currentCommunicationPoint=0.0, communicationStepSize=step_size
        )


def test_clock_does_not_drift_at_large_times(new_fmu) -> None:
    """The clock check uses a relative tolerance and follows the master clock."""
    start = 1.0e7
    fmu = new_fmu()
    fmu.setupExperiment(startTime=start)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    time = start
    for _ in range(200):
        assert do_step(fmu, time, 0.001) == FMI2_OK
        time += 0.001


@pytest.mark.parametrize("start", [0.0, 1.0e6])
@pytest.mark.parametrize("master", ["accumulated", "multiplied"])
def test_master_clock_rounding_is_silent(new_fmu, start, master) -> None:
    """The usual master clocks (t += h and t = t0 + k*h) differ from the FMU clock
    by rounding at most, so every step returns fmi2OK and no clock warning."""
    step_size = 1e-3
    fmu = new_fmu()
    fmu.setupExperiment(startTime=start)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    time = start
    for k in range(10_000):
        if master == "multiplied":
            time = start + k * step_size
        assert do_step(fmu, time, step_size) == FMI2_OK, f"step {k} at t = {time!r}"
        if master == "accumulated":
            time += step_size


def test_clock_difference_beyond_rounding_is_reported(new_fmu) -> None:
    """A mismatch within the tolerance but far above rounding is accepted with fmi2Warning."""
    log = []
    fmu = new_fmu(log=log)
    fmu.setupExperiment(startTime=1000.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    def clock_warnings() -> list:
        return [entry for entry in log if "differs from environment clock" in entry[2]]

    # tolerance at t = 1000 is 1e-6; the warning threshold for h = 1e-3 is 1e-9
    assert do_step(fmu, 1000.0 + 1e-7, 1e-3) == FMI2_WARNING
    warnings = clock_warnings()
    assert len(warnings) == 1, log
    assert warnings[0][:2] == (FMI2_WARNING, "logStatusWarning")

    # the FMU continued from the environment clock, so the next step matches exactly
    assert do_step(fmu, 1000.0 + 1e-7 + 1e-3, 1e-3) == FMI2_OK
    assert len(clock_warnings()) == 1


def test_model_exchange_is_rejected(unzipdir, model_description) -> None:
    """The FMU is Co-Simulation only: instantiating it as Model Exchange fails."""
    fmu = fmpy.fmi2.FMU2Model(
        guid=model_description.guid,
        unzipDirectory=unzipdir,
        modelIdentifier=model_description.coSimulation.modelIdentifier,
        instanceName="me_instance",
    )

    # The library must be unloaded here, since there is no instance to free
    try:
        with pytest.raises(Exception, match="Failed to instantiate model"):
            fmu.instantiate()
    finally:
        fmu.freeLibrary()


def test_unsupported_functions_fail(initialized_fmu, model_description) -> None:
    """Functions of capabilities the FMU does not have return fmi2Error."""
    assert not model_description.coSimulation.canInterpolateInputs

    with pytest.raises(FMICallException):
        initialized_fmu.cancelStep()


def test_unknown_log_category_is_ignored(new_fmu) -> None:
    """An unknown log category only causes a warning; the FMU stays usable."""
    fmu = new_fmu()
    fmu.setDebugLogging(loggingOn=True, categories=["logEvents", "noSuchCategory"])
    fmu.setupExperiment(startTime=0.0)


def test_status_queries(initialized_fmu) -> None:
    """Status of a synchronous slave: last step finished, clock, not terminated."""
    fmi2DoStepStatus, fmi2PendingStatus, fmi2LastSuccessfulTime, fmi2Terminated = range(
        4
    )

    initialized_fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.25)

    assert initialized_fmu.getStatus(fmi2DoStepStatus) == 0  # fmi2OK
    assert initialized_fmu.getRealStatus(fmi2LastSuccessfulTime) == 0.25
    assert initialized_fmu.getBooleanStatus(fmi2Terminated) == 0
    assert initialized_fmu.getStringStatus(fmi2PendingStatus) in (b"", "")

    # a status kind that is not available is answered with fmi2Discard,
    # which does not put the FMU into the error state
    with pytest.raises(FMICallException):
        initialized_fmu.getIntegerStatus(fmi2DoStepStatus)
    with pytest.raises(FMICallException):
        initialized_fmu.getRealStatus(fmi2Terminated)

    initialized_fmu.doStep(currentCommunicationPoint=0.25, communicationStepSize=0.25)
