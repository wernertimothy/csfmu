"""Command line interface of the csfmu project manager.

Usage:
    csfmu new <model> [--cpp] [--bare]
    csfmu rm <model>
    csfmu build [<model>] [-o <dir>]
    csfmu test [<model>] [-- <pytest args>]
    csfmu simulate [<model>] [-i <input.csv>] [-o <result.csv>] [--start <t>] [--stop <t>]
                   [--step <h>] [--tolerance <tol>] [--set <name>=<value> ...]
    csfmu list
    csfmu clean
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from .scaffold import create_model, validate_name
from .simulate import SimulationError, write_csv
from .simulate import simulate as simulate_fmu


def find_root() -> Path:
    """Return the project root: the first parent of the cwd with a CMakeLists.txt and src/models/."""
    cwd = Path.cwd().resolve()
    for directory in (cwd, *cwd.parents):
        if (directory / "CMakeLists.txt").is_file() and (
            directory / "src" / "models"
        ).is_dir():
            return directory
    raise SystemExit(
        "Error: not inside a csfmu project (no CMakeLists.txt with src/models/ found)"
    )


def available_models(root: Path) -> list[str]:
    """Return the sorted names of all buildable models under src/models/.

    A model is buildable if it has a variables.c and an equations.c or
    equations.cpp, which is the same check the CMakeLists.txt performs.
    """
    models = []
    for directory in (root / "src" / "models").iterdir():
        has_variables = (directory / "variables.c").is_file()
        has_equations = (directory / "equations.c").is_file() or (
            directory / "equations.cpp"
        ).is_file()
        if directory.is_dir() and has_variables and has_equations:
            models.append(directory.name)
    return sorted(models, key=str.lower)


def resolve_model(root: Path, name: str | None) -> str | None:
    """Return the model name if it exists, otherwise let the user select one.

    The selection is also shown if no name was given.
    Returns None if the user quits the selection.
    """
    models = available_models(root)
    if name in models:
        return name

    if name is None:
        print("Available models:")
    else:
        print(f"Error: model '{name}' not found in src/models/")
    for number, model in enumerate(models, start=1):
        print(f"  [{number}] {model}")

    while True:
        try:
            answer = input(f"Select a model [1-{len(models)}] or q to quit: ").strip()
        except EOFError, KeyboardInterrupt:
            print()
            return None
        if answer.lower() == "q":
            return None
        if answer.isdigit() and 1 <= int(answer) <= len(models):
            return models[int(answer) - 1]


def run(command: list[str], **kwargs) -> int:
    """Run a command, echoing it first, and return its exit code."""
    print("+", " ".join(command), flush=True)
    return subprocess.run(command, check=False, **kwargs).returncode


def require(program: str) -> None:
    """Exit with an error if the program is not on the PATH."""
    if shutil.which(program) is None:
        raise SystemExit(f"Error: '{program}' not found on PATH")


def build(root: Path, model: str, output_dir: Path | None = None) -> int:
    """Configure and build the model in build/<model>/; the FMU is placed in build/.

    If an output folder is given, the FMU is also copied there. It is not
    built there directly: the FMU is only packaged when the library is
    relinked (also after a change of the model's resources), so an
    up-to-date model would not be packaged again.
    """
    require("cmake")
    if output_dir is not None and output_dir.exists() and not output_dir.is_dir():
        print(f"Error: '{output_dir}' is not a folder")
        return 1
    build_dir = root / "build"
    model_build_dir = build_dir / model

    returncode = run(
        [
            "cmake",
            "-S",
            str(root),
            "-B",
            str(model_build_dir),
            f"-DMODEL={model}",
            f"-DFMU_OUTPUT_DIR={build_dir}",
        ]
    )
    if returncode != 0:
        return returncode
    returncode = run(["cmake", "--build", str(model_build_dir)])
    if (
        returncode != 0
        or output_dir is None
        or output_dir.resolve() == build_dir.resolve()
    ):
        return returncode

    output_dir.mkdir(parents=True, exist_ok=True)
    fmu = shutil.copy2(build_dir / f"{model}.fmu", output_dir)
    print(f"Copied FMU to {fmu}")
    return 0


def test(root: Path, model: str, pytest_args: list[str]) -> int:
    """Run the C unit tests and the pytest suite against build/<model>.fmu, building it if missing."""
    build_dir = root / "build"
    model_build_dir = build_dir / model

    if not (build_dir / f"{model}.fmu").is_file():
        returncode = build(root, model)
        if returncode != 0:
            return returncode

    if model_build_dir.is_dir():
        require("ctest")
        returncode = run(
            ["ctest", "--test-dir", str(model_build_dir), "--output-on-failure"]
        )
        if returncode != 0:
            return returncode
    else:
        print(f"Note: {model_build_dir} not found, skipping C unit tests")

    env = {**os.environ, "FMU_MODEL": model, "FMU_OUTPUT_DIR": str(build_dir)}
    return run([sys.executable, "-m", "pytest", *pytest_args], cwd=root, env=env)


def simulate(root: Path, model: str, args: argparse.Namespace) -> int:
    """Build the model, simulate its FMU and write the trajectory to a CSV file.

    The model is always built first. The build is incremental, so this costs
    little, and a result never comes from an FMU that is older than the
    sources (which would hide a change when two results are compared).
    """
    # Relative to where csfmu was called from, not to the project root
    output = (args.output or Path(f"{model}.csv")).expanduser().resolve()
    input_file = args.input.expanduser().resolve() if args.input else None
    if output.is_dir():
        print(f"Error: '{output}' is a folder, not a file")
        return 1
    if input_file is not None and not input_file.is_file():
        print(f"Error: input file '{input_file}' not found")
        return 1

    returncode = build(root, model)
    if returncode != 0:
        return returncode

    try:
        result = simulate_fmu(
            root / "build" / f"{model}.fmu",
            start=args.start,
            stop=args.stop,
            step=args.step,
            tolerance=args.tolerance,
            input_file=input_file,
            start_values=dict(args.start_values),
        )
    except SimulationError as error:
        print(f"Error: {error}")
        return 1

    if result.rows:
        output.parent.mkdir(parents=True, exist_ok=True)
        write_csv(output, result)
        print(
            f"Wrote {len(result.rows)} rows (t = {result.times[0]!r} to"
            f" {result.times[-1]!r}) to {output}"
        )
    if result.error is not None:
        print(f"Error: the simulation stopped at the {result.error}")
        return 1
    return 0


def new(root: Path, name: str, *, cpp: bool = False, bare: bool = False) -> int:
    """Create a new model from the template in src/models/<name>/ and its tests in tests/models/<name>/.

    With cpp, the equations are written in C++ (equations.cpp). With bare, the
    model is an empty skeleton that is not a valid model until it is filled in.
    """
    error = validate_name(name)
    if error is not None:
        print(f"Error: {error}")
        return 1
    # Names that only differ in case are the same folder on macOS and Windows,
    # so they count as the same model on every platform
    for parent in (root / "src" / "models", root / "tests" / "models"):
        for existing in parent.iterdir() if parent.is_dir() else ():
            if existing.name.lower() == name.lower():
                print(
                    f"Error: model '{name}' already exists ({existing.relative_to(root).as_posix()})"
                )
                return 1

    print(f"Created model '{name}':")
    for path in create_model(root, name, cpp=cpp, bare=bare):
        print(f"  {path.relative_to(root)}")
    print("Next:")
    if bare:
        print(
            "  fill in the model and delete what it does not need"
            " (it is not a valid model while empty)"
        )
    print(f"  uv run csfmu build {name}")
    print(f"  uv run csfmu test {name}")
    return 0


def model_traces(root: Path, name: str) -> list[Path]:
    """Return the existing files and folders that belong to the model only."""
    candidates = [
        root / "src" / "models" / name,
        root / "tests" / "models" / name,
        root / "examples" / f"example_{name}.py",
        root / "build" / name,
        root / "build" / f"{name}.fmu",
    ]
    # Compare the exact names: on macOS and Windows, path.exists() ignores case
    # and would let "rm First_Order" remove first_order
    return [
        path
        for path in candidates
        if path.exists() and path.name in os.listdir(path.parent)
    ]


def references_to(root: Path, name: str, excluded: list[Path]) -> list[Path]:
    """Return the text files of the project, other than the excluded ones, that mention the name as a word."""
    # Hidden folders are caches and tool settings, except for the CI workflows
    skipped_dirs = {"build", "external", "__pycache__"}
    pattern = re.compile(rf"\b{re.escape(name)}\b")
    references = []
    for directory, subdirs, files in os.walk(root):
        directory = Path(directory)
        subdirs[:] = sorted(
            d
            for d in subdirs
            if d not in skipped_dirs
            and (not d.startswith(".") or d == ".github")
            and directory / d not in excluded
        )
        for file in sorted(files):
            path = directory / file
            if path in excluded:
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError, OSError:
                continue
            if pattern.search(text):
                references.append(path)
    return references


def confirm(question: str) -> bool:
    """Ask a yes/no question that defaults to yes; no input (EOF) counts as no."""
    while True:
        try:
            answer = input(f"{question} [Y/n] ").strip().lower()
        except EOFError, KeyboardInterrupt:
            print()
            return False
        if answer in ("", "y", "yes"):
            return True
        if answer in ("n", "no"):
            return False


def remove(root: Path, name: str) -> int:
    """Remove all files and folders of a model after asking the user."""
    # The name must not be a path, otherwise it could point outside of src/models/
    if name in ("", ".", "..") or "/" in name or "\\" in name:
        print(f"Error: '{name}' is not a model name")
        return 1

    traces = model_traces(root, name)
    if not traces:
        print(f"Error: model '{name}' not found")
        print("Available models:")
        for model in available_models(root):
            print(f"  {model}")
        return 1

    def show(path: Path) -> str:
        return f"{path.relative_to(root).as_posix()}{'/' if path.is_dir() else ''}"

    print(f"This removes model '{name}':")
    for path in traces:
        print(f"  {show(path)}")
    references = references_to(root, name, traces)
    if references:
        print(f"These files still mention '{name}' and are not changed:")
        for path in references:
            print(f"  {show(path)}")

    if not confirm(f"Remove model '{name}'?"):
        print("Aborted, nothing was removed")
        return 1

    for path in traces:
        label = show(path)
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path)
        else:
            path.unlink()
        print(f"Removed {label}")
    return 0


def clean(root: Path) -> int:
    """Remove the build/ folder."""
    build_dir = root / "build"
    if not build_dir.exists():
        print("Nothing to clean")
        return 0
    shutil.rmtree(build_dir)
    print(f"Removed {build_dir}")
    return 0


def assignment(text: str) -> tuple[str, str]:
    """Split a NAME=VALUE argument into name and value."""
    name, separator, value = text.partition("=")
    if not separator or not name.strip():
        raise argparse.ArgumentTypeError(f"'{text}' is not of the form NAME=VALUE")
    return name.strip(), value


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="csfmu", description="Create, build and test csfmu FMUs."
    )
    commands = parser.add_subparsers(dest="command", required=True)

    new_parser = commands.add_parser("new", help="create a new model from a template")
    new_parser.add_argument(
        "model", help="name of the new folder under src/models/ (e.g. my_model)"
    )
    new_parser.add_argument(
        "--cpp",
        action="store_true",
        help="write the equations in C++ (equations.cpp instead of equations.c)",
    )
    new_parser.add_argument(
        "--bare",
        action="store_true",
        help="create an empty skeleton without equations (not a valid model until filled in)",
    )

    rm_parser = commands.add_parser("rm", help="remove a model and all its files")
    rm_parser.add_argument("model", help="name of a folder under src/models/")

    build_parser = commands.add_parser("build", help="build the FMU of a model")
    build_parser.add_argument(
        "model",
        nargs="?",
        help="name of a folder under src/models/ (prompted if omitted)",
    )
    build_parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        help="also place the FMU in this folder (created if missing)",
    )

    test_parser = commands.add_parser(
        "test", help="run the tests of a model (builds it if needed)"
    )
    test_parser.add_argument(
        "model",
        nargs="?",
        help="name of a folder under src/models/ (prompted if omitted)",
    )
    test_parser.epilog = "arguments after -- are passed to pytest"

    simulate_parser = commands.add_parser(
        "simulate",
        help="simulate a model and write its trajectory to a CSV file (builds it first)",
    )
    simulate_parser.add_argument(
        "model",
        nargs="?",
        help="name of a folder under src/models/ (prompted if omitted)",
    )
    simulate_parser.add_argument(
        "-i",
        "--input",
        type=Path,
        help="CSV file with the inputs over time: a column time, then one column per input",
    )
    simulate_parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="CSV file the trajectory is written to (default: <model>.csv)",
    )
    for option, what in (
        ("--start", "start time"),
        ("--stop", "stop time"),
        ("--step", "communication step size"),
    ):
        simulate_parser.add_argument(
            option,
            type=float,
            help=f"{what} (default: from the FMU's default experiment)",
        )
    simulate_parser.add_argument(
        "--tolerance",
        type=float,
        help="relative tolerance for the FMU (default: the FMU's own)",
    )
    simulate_parser.add_argument(
        "--set",
        dest="start_values",
        type=assignment,
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="set a parameter, input or state before the initialization (repeatable)",
    )

    commands.add_parser("list", help="list the available models")
    commands.add_parser("clean", help="remove the build/ folder")

    # Split off the pytest arguments before parsing, otherwise argparse would
    # take the first one as the (optional) model name.
    argv = sys.argv[1:] if argv is None else argv
    pytest_args = []
    if "--" in argv:
        separator = argv.index("--")
        argv, pytest_args = argv[:separator], argv[separator + 1 :]

    args = parser.parse_args(argv)
    args.pytest_args = pytest_args
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    root = find_root()

    if args.command == "list":
        print("\n".join(available_models(root)))
        return 0
    if args.command == "clean":
        return clean(root)
    if args.command == "new":
        return new(root, args.model, cpp=args.cpp, bare=args.bare)
    if args.command == "rm":
        return remove(root, args.model)

    model = resolve_model(root, args.model)
    if model is None:
        return 1
    if args.command == "build":
        # Relative to where csfmu was called from, not to the project root
        output_dir = args.output_dir.expanduser().resolve() if args.output_dir else None
        return build(root, model, output_dir)
    if args.command == "simulate":
        return simulate(root, model, args)
    return test(root, model, args.pytest_args)


if __name__ == "__main__":
    sys.exit(main())
