import os
import pytest
from pathlib import Path

@pytest.fixture(scope="session")
def path_to_project_root() -> Path:
    """ Return a path object of the project root directory. """
    file_path = Path(__file__).parent
    return file_path.parent

@pytest.fixture(scope="session")
def path_to_fmu(path_to_project_root : Path) -> Path:
    """ Return a path object to the FMU.

    The model name and output directory can be overridden via environment
    variables for multi-model testing:
        FMU_MODEL      - model name (default: "first_order")
        FMU_OUTPUT_DIR - directory containing the .fmu (default: "build/")
    """
    model_name = os.environ.get("FMU_MODEL", "first_order")
    fmu_dir = os.environ.get("FMU_OUTPUT_DIR", str(path_to_project_root / "build"))
    path_to_fmu = Path(fmu_dir) / f"{model_name}.fmu"
    assert path_to_fmu.is_file(), f"FMU not found at {path_to_fmu}"
    return path_to_fmu
