import shutil
import subprocess
import sys
from pathlib import Path

import pytest


@pytest.mark.skipif(
    sys.platform == "win32" or shutil.which("nm") is None, reason="requires nm"
)
def test_only_fmi2_functions_are_exported(unzipdir: Path, model_description) -> None:
    """
    Static test: Verify the FMU binary exports nothing but the fmi2* functions.

    Exported internals (MODEL_DEFINITION, newModel, ...) would let two FMUs of
    this template collide when an importer loads them into one process.
    """
    model_identifier = model_description.coSimulation.modelIdentifier
    binaries = [
        path
        for path in unzipdir.glob(f"binaries/*/{model_identifier}.*")
        if path.suffix in (".so", ".dylib")
    ]
    assert binaries, (
        f"No binary for '{model_identifier}' found in {unzipdir / 'binaries'}"
    )

    # macOS nm has no --defined-only, its equivalent is -U
    flags = ["-gU"] if sys.platform == "darwin" else ["-g", "--defined-only"]

    for binary in binaries:
        output = subprocess.run(
            ["nm", *flags, str(binary)], capture_output=True, text=True, check=True
        ).stdout

        # Each line is "<address> <type> <name>"; macOS prefixes C names with an underscore
        symbols = [line.split()[-1] for line in output.splitlines() if line.strip()]
        if sys.platform == "darwin":
            symbols = [symbol.removeprefix("_") for symbol in symbols]

        assert "fmi2Instantiate" in symbols, (
            f"{binary.name} does not export fmi2Instantiate"
        )

        internals = [symbol for symbol in symbols if not symbol.startswith("fmi2")]
        assert not internals, f"{binary.name} exports internals: {internals}"
