import pytest
import fmpy
from pathlib import Path

def test_fmu_model_description_loads(path_to_fmu : Path) -> None:
    """Static test: Verify modelDescription.xml can be parsed"""
    model_description = fmpy.read_model_description(path_to_fmu)
    assert model_description is not None