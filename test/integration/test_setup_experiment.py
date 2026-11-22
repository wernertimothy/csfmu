"""Model-independent tests of fmi2SetupExperiment: the tolerance and the time window.

A defined tolerance becomes the model's relative tolerance, otherwise the
model's default (defaultExperiment tolerance) applies. The value is not
published as a variable, so the tests read it from the log.
"""

import math

import pytest
from fmpy.fmi1 import FMICallException
from fmpy.fmi2 import fmi2Error, fmi2OK, fmi2Warning

from test.fmu_helpers import advance, initialize


def setup_messages(log: list) -> list:
    """The log messages of fmi2SetupExperiment."""
    return [message for _, _, message in log if "Experiment set up" in message]


def default_tolerance(model_description) -> float:
    """The tolerance the FMU uses if the integration tool defines none."""
    tolerance = model_description.defaultExperiment.tolerance
    return 1e-6 if tolerance is None else float(tolerance)


def test_defined_tolerance_is_used(new_fmu) -> None:
    """A tolerance from the integration tool is accepted and logged."""
    log = []
    fmu = new_fmu(log=log)

    status = fmu.setupExperiment(tolerance=1e-8, startTime=0.0)

    assert status == fmi2OK
    assert "Tolerance = 1e-08 (set by the integration tool)" in setup_messages(log)[-1]

    # the FMU simulates as usual
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    advance(fmu, 0.03)


def test_default_tolerance_without_definition(new_fmu, model_description) -> None:
    """Without a tolerance the model's default applies."""
    log = []
    fmu = new_fmu(log=log)

    fmu.setupExperiment(startTime=0.0)

    expected = f"Tolerance = {default_tolerance(model_description):g} (default)"
    assert expected in setup_messages(log)[-1]


def test_later_setup_without_tolerance_falls_back_to_the_default(
    new_fmu, model_description
) -> None:
    """fmi2SetupExperiment may be called again before initialization; the last call counts."""
    log = []
    fmu = new_fmu(log=log)

    fmu.setupExperiment(tolerance=1e-8, startTime=0.0)
    fmu.setupExperiment(startTime=0.0)

    expected = f"Tolerance = {default_tolerance(model_description):g} (default)"
    assert expected in setup_messages(log)[-1]


def test_reset_restores_the_default_tolerance(new_fmu, model_description) -> None:
    """After fmi2Reset the FMU uses its default tolerance again."""
    log = []
    fmu = initialize(new_fmu(log=log), tolerance=1e-8)

    fmu.reset()
    fmu.setupExperiment(startTime=0.0)

    expected = f"Tolerance = {default_tolerance(model_description):g} (default)"
    assert expected in setup_messages(log)[-1]


@pytest.mark.parametrize("tolerance", [0.0, -1e-6, math.nan, math.inf])
def test_invalid_tolerance_falls_back_to_the_default(
    new_fmu, model_description, tolerance
) -> None:
    """A defined tolerance that is not finite and > 0 is ignored with fmi2Warning."""
    log = []
    fmu = new_fmu(log=log)

    status = fmu.setupExperiment(tolerance=tolerance, startTime=0.0)

    assert status == fmi2Warning
    ignored = [entry for entry in log if "is ignored" in entry[2]]
    assert len(ignored) == 1, log
    assert ignored[0][:2] == (fmi2Warning, "logStatusWarning")
    expected = f"Tolerance = {default_tolerance(model_description):g} (default)"
    assert expected in setup_messages(log)[-1]

    # the experiment continues
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)


@pytest.mark.parametrize(
    "start_time, stop_time",
    [(math.nan, None), (math.inf, None), (1.0, 0.5), (0.0, math.nan)],
    ids=["start_nan", "start_inf", "stop_before_start", "stop_nan"],
)
def test_invalid_time_window_is_rejected(new_fmu, start_time, stop_time) -> None:
    """startTime and stopTime define the experiment, so there is no fallback for them."""
    fmu = new_fmu()

    with pytest.raises(FMICallException) as failure:
        fmu.setupExperiment(startTime=start_time, stopTime=stop_time)

    assert failure.value.status == fmi2Error
