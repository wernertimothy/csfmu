"""Command line interface of the csfmu project manager.

Usage:
    csfmu build [<model>]
    csfmu test [<model>] [-- <pytest args>]
    csfmu list
    csfmu clean
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def find_root() -> Path:
    """ Return the project root: the first parent of the cwd with a CMakeLists.txt and src/models/. """
    cwd = Path.cwd().resolve()
    for directory in (cwd, *cwd.parents):
        if (directory / "CMakeLists.txt").is_file() and (directory / "src" / "models").is_dir():
            return directory
    raise SystemExit("Error: not inside a csfmu project (no CMakeLists.txt with src/models/ found)")


def available_models(root: Path) -> list[str]:
    """ Return the sorted names of all buildable models under src/models/.

    A model is buildable if it has a variables.c and an equations.c or
    equations.cpp, which is the same check the CMakeLists.txt performs.
    """
    models = []
    for directory in (root / "src" / "models").iterdir():
        has_variables = (directory / "variables.c").is_file()
        has_equations = (directory / "equations.c").is_file() or (directory / "equations.cpp").is_file()
        if directory.is_dir() and has_variables and has_equations:
            models.append(directory.name)
    return sorted(models)


def resolve_model(root: Path, name: str | None) -> str | None:
    """ Return the model name if it exists, otherwise let the user select one.

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
        except (EOFError, KeyboardInterrupt):
            print()
            return None
        if answer.lower() == "q":
            return None
        if answer.isdigit() and 1 <= int(answer) <= len(models):
            return models[int(answer) - 1]


def run(command: list[str], **kwargs) -> int:
    """ Run a command, echoing it first, and return its exit code. """
    print("+", " ".join(command), flush=True)
    return subprocess.run(command, **kwargs).returncode


def require(program: str) -> None:
    """ Exit with an error if the program is not on the PATH. """
    if shutil.which(program) is None:
        raise SystemExit(f"Error: '{program}' not found on PATH")


def build(root: Path, model: str) -> int:
    """ Configure and build the model in build/<model>/; the FMU is placed in build/. """
    require("cmake")
    build_dir = root / "build"
    model_build_dir = build_dir / model

    returncode = run([
        "cmake",
        "-S", str(root),
        "-B", str(model_build_dir),
        f"-DMODEL={model}",
        f"-DFMU_OUTPUT_DIR={build_dir}",
    ])
    if returncode != 0:
        return returncode
    return run(["cmake", "--build", str(model_build_dir)])


def test(root: Path, model: str, pytest_args: list[str]) -> int:
    """ Run the C unit tests and the pytest suite against build/<model>.fmu, building it if missing. """
    build_dir = root / "build"
    model_build_dir = build_dir / model

    if not (build_dir / f"{model}.fmu").is_file():
        returncode = build(root, model)
        if returncode != 0:
            return returncode

    if model_build_dir.is_dir():
        require("ctest")
        returncode = run(["ctest", "--test-dir", str(model_build_dir), "--output-on-failure"])
        if returncode != 0:
            return returncode
    else:
        print(f"Note: {model_build_dir} not found, skipping C unit tests")

    env = {**os.environ, "FMU_MODEL": model, "FMU_OUTPUT_DIR": str(build_dir)}
    return run([sys.executable, "-m", "pytest", *pytest_args], cwd=root, env=env)


def clean(root: Path) -> int:
    """ Remove the build/ folder. """
    build_dir = root / "build"
    if not build_dir.exists():
        print("Nothing to clean")
        return 0
    shutil.rmtree(build_dir)
    print(f"Removed {build_dir}")
    return 0


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="csfmu", description="Build and test csfmu FMUs.")
    commands = parser.add_subparsers(dest="command", required=True)

    build_parser = commands.add_parser("build", help="build the FMU of a model")
    build_parser.add_argument("model", nargs="?", help="name of a folder under src/models/ (prompted if omitted)")

    test_parser = commands.add_parser("test", help="run the tests of a model (builds it if needed)")
    test_parser.add_argument("model", nargs="?", help="name of a folder under src/models/ (prompted if omitted)")
    test_parser.epilog = "arguments after -- are passed to pytest"

    commands.add_parser("list", help="list the available models")
    commands.add_parser("clean", help="remove the build/ folder")

    # Split off the pytest arguments before parsing, otherwise argparse would
    # take the first one as the (optional) model name.
    argv = sys.argv[1:] if argv is None else argv
    pytest_args = []
    if "--" in argv:
        separator = argv.index("--")
        argv, pytest_args = argv[:separator], argv[separator + 1:]

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

    model = resolve_model(root, args.model)
    if model is None:
        return 1
    if args.command == "build":
        return build(root, model)
    return test(root, model, args.pytest_args)


if __name__ == "__main__":
    sys.exit(main())
