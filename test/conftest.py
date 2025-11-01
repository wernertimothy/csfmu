import pytest
from pathlib import Path

@pytest.fixture(scope="session")
def path_to_project_root() -> Path:
    file_path = Path(__file__).parent
    return file_path.parent

@pytest.fixture(scope="session")
def path_to_fmu(path_to_project_root : Path) -> Path:
    path_to_fmu = path_to_project_root.joinpath("build","fmuTemplate.fmu")
    assert path_to_fmu.is_file()
    return path_to_fmu
