import os
import subprocess
import sys
from pathlib import Path

import fmpy
import pytest

# The two models are loaded in a separate process, because a library loaded
# with RTLD_GLOBAL stays visible to everything loaded afterwards and would
# influence the other tests.
LOAD_TWO_FMUS = """
import ctypes
import sys
from pathlib import Path
import fmpy
from fmpy.fmi2 import FMU2Slave

first_dir, second_dir = sys.argv[1:3]

# Make the symbols of the first FMU visible to the whole process
first = fmpy.read_model_description(first_dir).coSimulation.modelIdentifier
binaries = [path for path in Path(first_dir).glob(f"binaries/*/{first}.*") if path.suffix in (".so", ".dylib", ".dll")]
for binary in binaries:
    ctypes.CDLL(str(binary), mode=ctypes.RTLD_GLOBAL)
assert binaries, f"no binary found for {first}"

# The second FMU must still use its own model definition (GUID check)
model_description = fmpy.read_model_description(second_dir)
fmu = FMU2Slave(
    guid=model_description.guid,
    unzipDirectory=second_dir,
    modelIdentifier=model_description.coSimulation.modelIdentifier,
    instanceName="second",
)
fmu.instantiate()
fmu.setupExperiment(startTime=0.0)
fmu.enterInitializationMode()
fmu.exitInitializationMode()
fmu.doStep(currentCommunicationPoint=0.0, communicationStepSize=0.01)
fmu.terminate()
fmu.freeInstance()
"""


def test_two_fmus_in_one_process(path_to_project_root : Path, tmp_path : Path) -> None:
    """
    Verify that an FMU still runs its own model if another FMU of this
    template was loaded into the process with RTLD_GLOBAL before.

    Needs the FMUs of two different models. The test is skipped if they have
    not been built, unless CSFMU_REQUIRE_TWO_FMUS is set (CI).
    """
    fmu_dir = Path(os.environ.get("FMU_OUTPUT_DIR", str(path_to_project_root / "build")))
    fmus = [fmu_dir / "mass_spring_damper.fmu", fmu_dir / "typed_demo.fmu"]

    missing = [fmu.name for fmu in fmus if not fmu.is_file()]
    if missing:
        message = f"FMUs not built: {missing}"
        if os.environ.get("CSFMU_REQUIRE_TWO_FMUS"):
            pytest.fail(message)
        pytest.skip(message)

    unzipdirs = [fmpy.extract(fmu, tmp_path / fmu.stem) for fmu in fmus]

    result = subprocess.run([sys.executable, "-c", LOAD_TWO_FMUS, *map(str, unzipdirs)], capture_output=True, text=True)
    assert result.returncode == 0, f"Second FMU failed after the first was loaded globally:\n{result.stdout}\n{result.stderr}"
