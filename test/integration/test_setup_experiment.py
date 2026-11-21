import math
import pytest

"""
Model-independent tests of fmi2SetupExperiment: the tolerance.

A defined tolerance becomes the model's relative tolerance, otherwise the
model's default (defaultExperiment tolerance) applies. The value is not
published as a variable, so the tests read it from the log.
"""


def setup_messages(log: list) -> list:
    """ The log messages of fmi2SetupExperiment. """
    return [message for _, _, message in log if "Experiment set up" in message]


def default_tolerance(model_description) -> float:
    """ The tolerance the FMU uses if the integration tool defines none. """
    tolerance = model_description.defaultExperiment.tolerance
    return 1e-6 if tolerance is None else float(tolerance)


def test_defined_tolerance_is_used(new_fmu) -> None:
    """ A tolerance from the integration tool is accepted and logged. """
    log = []
    fmu = new_fmu(log=log)

    fmu.setupExperiment(tolerance=1e-8, startTime=0.0)

    assert "Tolerance = 1e-08 (set by the integration tool)" in setup_messages(log)[-1]

    # the FMU simulates as usual
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    for k in range(3):
        fmu.doStep(currentCommunicationPoint=k * 0.01, communicationStepSize=0.01)


def test_default_tolerance_without_definition(new_fmu, model_description) -> None:
    """ Without a tolerance the model's default applies. """
    log = []
    fmu = new_fmu(log=log)

    fmu.setupExperiment(startTime=0.0)

    expected = f"Tolerance = {default_tolerance(model_description):g} (default)"
    assert expected in setup_messages(log)[-1]


def test_later_setup_without_tolerance_falls_back_to_the_default(new_fmu, model_description) -> None:
    """ fmi2SetupExperiment may be called again before initialization; the last call counts. """
    log = []
    fmu = new_fmu(log=log)

    fmu.setupExperiment(tolerance=1e-8, startTime=0.0)
    fmu.setupExperiment(startTime=0.0)

    expected = f"Tolerance = {default_tolerance(model_description):g} (default)"
    assert expected in setup_messages(log)[-1]


def test_reset_restores_the_default_tolerance(new_fmu, model_description) -> None:
    """ After fmi2Reset the FMU uses its default tolerance again. """
    log = []
    fmu = new_fmu(log=log)
    fmu.setupExperiment(tolerance=1e-8, startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()

    fmu.reset()
    fmu.setupExperiment(startTime=0.0)

    expected = f"Tolerance = {default_tolerance(model_description):g} (default)"
    assert expected in setup_messages(log)[-1]


@pytest.mark.parametrize("tolerance", [0.0, -1e-6, math.nan, math.inf])
def test_invalid_tolerance_is_rejected(new_fmu, tolerance) -> None:
    """ A defined tolerance must be finite and greater than zero. """
    fmu = new_fmu()

    with pytest.raises(Exception):
        fmu.setupExperiment(tolerance=tolerance, startTime=0.0)
