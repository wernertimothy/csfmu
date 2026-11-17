import ctypes
import fmpy
from fmpy.validation import validate_fmu
from pathlib import Path

FMI2_OK = 0


def test_fmu_model_description_is_valid(path_to_fmu : Path) -> None:
    """Static test: Verify modelDescription.xml against the FMI 2.0 schema and rules"""
    model_description = fmpy.read_model_description(
        path_to_fmu, validate=True, validate_variable_names=True, validate_model_structure=True)
    assert model_description is not None

    problems = validate_fmu(path_to_fmu)
    assert not problems, f"FMPy reports problems: {problems}"


def test_model_identifier_is_the_name_of_the_fmu(path_to_fmu : Path, model_description) -> None:
    """The binary is found by the model identifier, which is also the name of the FMU"""
    assert model_description.coSimulation.modelIdentifier == path_to_fmu.stem
    assert model_description.modelName == path_to_fmu.stem


def test_declared_log_categories_are_known(new_fmu, model_description) -> None:
    """Every log category of the modelDescription.xml is accepted by the FMU.

    The list in the modelDescription.xml and the one in fmi2Functions.c are
    maintained by hand; an unknown category is answered with fmi2Warning.
    """
    fmu = new_fmu()
    names = [category.name for category in model_description.logCategories]
    assert names, "the modelDescription.xml declares no log categories"

    for name in names:
        categories = (ctypes.c_char_p * 1)(name.encode("utf-8"))
        status = fmu.dll.fmi2SetDebugLogging(fmu.component, 1, 1, categories)
        assert status == FMI2_OK, f"log category '{name}' is not known to the FMU"
