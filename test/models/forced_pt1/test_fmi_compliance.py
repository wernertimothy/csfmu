import fmpy
import pytest
from fmpy.validation import validate_fmu
from pathlib import Path


@pytest.fixture(autouse=True)
def only_forced_pt1(model_description) -> None:
    """ Skip these tests if the FMU under test is not the forced_pt1 model. """
    if model_description.modelName != "forced_pt1":
        pytest.skip(f"Test specific to forced_pt1 model, got {model_description.modelName}")


def test_model_description_is_valid(path_to_fmu: Path) -> None:
    """ The modelDescription.xml follows the FMI 2.0 schema, variable naming and model structure rules. """
    model_description = fmpy.read_model_description(
        path_to_fmu, validate=True, validate_variable_names=True, validate_model_structure=True)
    assert model_description is not None


def test_fmu_is_valid(path_to_fmu: Path) -> None:
    """ FMPy finds no problems in the FMU. """
    problems = validate_fmu(path_to_fmu)
    assert not problems, f"FMPy reports problems: {problems}"
