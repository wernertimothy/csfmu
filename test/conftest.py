import os
from ctypes import byref
from pathlib import Path

import fmpy
import fmpy.fmi2
import fmpy.logging
import pytest


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


@pytest.fixture(scope="session")
def unzipdir(path_to_fmu : Path, tmp_path_factory) -> Path:
    """ Return a path object to the extracted FMU.

    The FMU is extracted once per test session. Extracting it again would
    overwrite the binary, which must not happen while it is loaded: Windows
    refuses it and Linux crashes.
    """
    return Path(fmpy.extract(path_to_fmu, tmp_path_factory.mktemp("fmu")))


@pytest.fixture(scope="session")
def model_description(path_to_fmu : Path):
    """ Return the parsed modelDescription.xml of the FMU. """
    return fmpy.read_model_description(path_to_fmu)

@pytest.fixture(scope="session")
def vr(model_description):
    """ Return a function that maps a variable name to its value reference.

    Value references are assigned by the build (index in the model's variable
    table), so tests must never hardcode them.
    """
    value_references = {v.name: v.valueReference for v in model_description.modelVariables}

    def value_reference_of(name : str) -> int:
        assert name in value_references, f"FMU has no variable named '{name}'"
        return value_references[name]

    return value_reference_of

def logging_callbacks(log : list) -> fmpy.fmi2.fmi2CallbackFunctions:
    """ Return FMI callbacks whose logger appends every message to log.

    The FMU passes its messages as "%s" plus a variadic argument, which ctypes
    cannot hand to Python. FMPy's native logger proxy formats the message
    first. The FMU keeps a reference to the callbacks, so they stay alive.
    """
    def logger(component, instanceName, status, category, message):
        log.append((status, category.decode("utf-8"), message.decode("utf-8")))

    callbacks = fmpy.fmi2.fmi2CallbackFunctions()
    callbacks.logger = fmpy.fmi2.fmi2CallbackLoggerTYPE(logger)
    callbacks.allocateMemory = fmpy.fmi2.fmi2CallbackAllocateMemoryTYPE(fmpy.calloc)
    callbacks.freeMemory = fmpy.fmi2.fmi2CallbackFreeMemoryTYPE(fmpy.free)
    fmpy.logging.addLoggerProxy(byref(callbacks))
    return callbacks

@pytest.fixture
def new_fmu(unzipdir : Path, model_description):
    """ Return a function that creates an instantiated FMU.

    All instances created by a test are freed after the test, also if the
    test left the FMU in an error state.
    """
    instances = []

    def instantiate(instance_name : str = "test_instance", log : list | None = None):
        """ Create an FMU. If a list is given as log, logging is switched on
        and every message is appended to it as (status, category, message). """
        fmu = fmpy.fmi2.FMU2Slave(
            guid=model_description.guid,
            unzipDirectory=unzipdir,
            modelIdentifier=model_description.coSimulation.modelIdentifier,
            instanceName=instance_name
        )
        if log is None:
            fmu.instantiate()
        else:
            fmu.instantiate(callbacks=logging_callbacks(log), loggingOn=True)
        instances.append(fmu)
        return fmu

    yield instantiate

    for fmu in instances:
        fmu.freeInstance()

@pytest.fixture
def initialized_fmu(new_fmu):
    """ Return an FMU that has completed initialization with default values. """
    fmu = new_fmu()
    fmu.setupExperiment(startTime=0.0)
    fmu.enterInitializationMode()
    fmu.exitInitializationMode()
    return fmu
