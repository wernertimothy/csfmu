"""Tests of the csfmu command line tool that need neither CMake nor an FMU.

They work on a fake project in a temporary folder and run once, not per
model: uv run pytest tools/tests. That a scaffolded model builds and passes
its tests is checked by the CLI job of the CI.
"""

import re
from pathlib import Path

import pytest
from csfmu import cli
from csfmu.scaffold import camel_case, create_model, validate_name

# The root of this repository (not of the fake project)
REPO = Path(__file__).resolve().parents[2]

# The storage blocks of a model, in the order of the bare template
STORAGE_STRUCTS = ["Params", "Inputs", "Outputs", "Locals", "States", "Internals"]


@pytest.fixture
def project(tmp_path):
    """An empty csfmu project: a CMakeLists.txt and the model folders."""
    (tmp_path / "CMakeLists.txt").write_text("")
    (tmp_path / "src" / "models").mkdir(parents=True)
    (tmp_path / "tests" / "models").mkdir(parents=True)
    return tmp_path


def add_model(root, name: str, equations: str = "equations.c") -> None:
    """Add the files that make a folder under src/models/ a buildable model."""
    folder = root / "src" / "models" / name
    folder.mkdir()
    (folder / "variables.c").write_text("")
    (folder / equations).write_text("")


def answer(monkeypatch, *answers: str) -> None:
    """Answer the next questions of the tool with the given input."""
    replies = iter(answers)
    monkeypatch.setattr("builtins.input", lambda prompt="": next(replies))


def strip_comments(source: str) -> str:
    """Remove the /* */ and // comments of C or C++ source code."""
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def model_definition_fields() -> list[str]:
    """Return the field names of the struct modelDefinition in include/model.h of this repository."""
    header = strip_comments((REPO / "include" / "model.h").read_text(encoding="utf-8"))
    body = re.search(r"typedef struct \{([^{}]*)\} modelDefinition;", header).group(1)
    return re.findall(r"(\w+)\s*;", body)


# argument parsing


def test_pytest_arguments_follow_the_separator() -> None:
    args = cli.parse_args(["test", "my_model", "--", "-k", "state", "-v"])

    assert args.model == "my_model"
    assert args.pytest_args == ["-k", "state", "-v"]


def test_pytest_arguments_do_not_become_the_model() -> None:
    """Without a model before --, the model is prompted for, not taken from pytest's arguments."""
    args = cli.parse_args(["test", "--", "-v"])

    assert args.model is None
    assert args.pytest_args == ["-v"]


def test_new_writes_c_unless_cpp_is_given() -> None:
    assert cli.parse_args(["new", "my_model"]).cpp is False
    assert cli.parse_args(["new", "my_model", "--cpp"]).cpp is True


def test_new_writes_the_template_unless_bare_is_given() -> None:
    assert cli.parse_args(["new", "my_model"]).bare is False

    args = cli.parse_args(["new", "my_model", "--bare", "--cpp"])
    assert args.bare is True
    assert args.cpp is True


# finding the project and its models


def test_root_is_found_from_a_subfolder(project, monkeypatch) -> None:
    monkeypatch.chdir(project / "tests" / "models")
    assert cli.find_root() == project.resolve()


def test_outside_a_project_is_an_error(tmp_path, monkeypatch) -> None:
    monkeypatch.chdir(tmp_path)
    with pytest.raises(SystemExit):
        cli.find_root()


def test_available_models_need_variables_and_equations(project) -> None:
    add_model(project, "beta")
    add_model(project, "Alpha", equations="equations.cpp")
    (project / "src" / "models" / "incomplete").mkdir()
    (project / "src" / "models" / "incomplete" / "variables.c").write_text("")
    (project / "src" / "models" / "README.md").write_text("")

    assert cli.available_models(project) == ["Alpha", "beta"]


def test_existing_model_is_resolved_without_asking(project, monkeypatch) -> None:
    add_model(project, "first_order")
    answer(monkeypatch)  # any question would fail the test

    assert cli.resolve_model(project, "first_order") == "first_order"


def test_unknown_model_is_selected_by_number(project, monkeypatch) -> None:
    add_model(project, "first_order")
    add_model(project, "second_order")
    answer(monkeypatch, "7", "2")  # out of range, then valid

    assert cli.resolve_model(project, "no_such_model") == "second_order"


def test_selection_can_be_quit(project, monkeypatch) -> None:
    add_model(project, "first_order")
    answer(monkeypatch, "q")

    assert cli.resolve_model(project, None) is None


# model names


@pytest.mark.parametrize(
    "name, expected",
    [("first_order", "firstOrder"), ("my_PID", "myPID"), ("model", "model")],
)
def test_camel_case(name, expected) -> None:
    assert camel_case(name) == expected


@pytest.mark.parametrize("name", ["my_model", "PID2", "a" * 64])
def test_valid_names(name) -> None:
    assert validate_name(name) is None


@pytest.mark.parametrize(
    "name",
    [
        "1model",  # starts with a digit
        "my-model",  # not an identifier
        "my__model",  # double underscore
        "model_",  # trailing underscore
        "a" * 65,  # too long
        "int",  # C keyword
        "class",  # C++ and Python keyword
        "test",  # CMake target
        "CON",  # reserved by Windows
    ],
)
def test_invalid_names(name) -> None:
    assert validate_name(name) is not None


# new


def test_new_model_has_sources_and_a_test(project) -> None:
    files = create_model(project, "my_model")

    assert [path.relative_to(project).as_posix() for path in files] == [
        "src/models/my_model/variables.h",
        "src/models/my_model/variables.c",
        "src/models/my_model/equations.c",
        "tests/models/my_model/__init__.py",
        "tests/models/my_model/test_my_model.py",
    ]
    assert cli.available_models(project) == ["my_model"]

    variables_c = (project / "src/models/my_model/variables.c").read_text()
    assert '.name        = "my_model"' in variables_c
    assert "myModelSystem" in variables_c


def test_new_cpp_model_has_cpp_equations(project) -> None:
    files = create_model(project, "my_model", cpp=True)

    assert [path.relative_to(project).as_posix() for path in files] == [
        "src/models/my_model/variables.h",
        "src/models/my_model/variables.c",
        "src/models/my_model/equations.cpp",
        "tests/models/my_model/__init__.py",
        "tests/models/my_model/test_my_model.py",
    ]
    assert not (project / "src/models/my_model/equations.c").exists()
    assert cli.available_models(project) == ["my_model"]

    equations = (project / "src/models/my_model/equations.cpp").read_text()
    assert 'extern "C" {' in equations
    assert "void myModelSystem(modelInstance* model)" in equations
    assert "void myModelOutputs(modelInstance* model)" in equations


def test_new_passes_cpp_on(project) -> None:
    assert cli.new(project, "my_model", cpp=True) == 0
    assert (project / "src/models/my_model/equations.cpp").is_file()


def test_bare_model_has_every_storage_block_empty(project) -> None:
    """The bare template has the same files, and its structs have no fields."""
    files = create_model(project, "my_model", bare=True)

    assert [path.relative_to(project).as_posix() for path in files] == [
        "src/models/my_model/variables.h",
        "src/models/my_model/variables.c",
        "src/models/my_model/equations.c",
        "tests/models/my_model/__init__.py",
        "tests/models/my_model/test_my_model.py",
    ]
    header = strip_comments((project / "src/models/my_model/variables.h").read_text())
    structs = re.findall(r"typedef struct \{(.*?)\} (\w+);", header, flags=re.DOTALL)
    assert [name for _, name in structs] == STORAGE_STRUCTS
    assert [body.strip() for body, _ in structs] == [""] * len(STORAGE_STRUCTS)


def test_bare_model_definition_sets_every_field(project) -> None:
    """The bare variables.c spells out every field of the model definition.

    Unlike the other tests, this one reads include/model.h of this
    repository: it fails when the model definition gets a field that the
    bare template does not set yet.
    """
    create_model(project, "my_model", bare=True)
    source = strip_comments((project / "src/models/my_model/variables.c").read_text())

    fields = model_definition_fields()
    assert "getStateDerivatives" in fields, "include/model.h was not parsed"
    missing = [field for field in fields if not re.search(rf"\.{field}\s*=", source)]
    assert missing == []


@pytest.mark.parametrize("cpp", [False, True], ids=["C", "C++"])
def test_bare_functions_are_empty(project, cpp) -> None:
    """Apart from comments, the bare functions only use their argument (and return SIM_OK)."""
    files = create_model(project, "my_model", cpp=cpp, bare=True)
    equations = next(path for path in files if path.stem == "equations")
    assert equations.suffix == (".cpp" if cpp else ".c")

    source = strip_comments(equations.read_text())
    bodies = re.findall(
        r"(\w+)\(modelInstance\* model\)\s*\{(.*?)\}", source, flags=re.DOTALL
    )
    assert {function: " ".join(body.split()) for function, body in bodies} == {
        "myModelSystem": "(void)model;",
        "myModelOutputs": "(void)model;",
        "myModelCalculateParameters": "(void)model;",
        "myModelInitialize": "(void)model; return SIM_OK;",
    }

    # every function is declared and part of the model definition
    header = (project / "src/models/my_model/variables.h").read_text()
    definition = (project / "src/models/my_model/variables.c").read_text()
    for function, _ in bodies:
        assert f"{function}(modelInstance* model);" in header
        assert f"= {function}," in definition


def test_bare_test_file_has_no_tests(project) -> None:
    create_model(project, "my_model", bare=True)
    text = (project / "tests/models/my_model/test_my_model.py").read_text()

    compile(text, "test_my_model.py", "exec")  # valid Python
    assert text.startswith('"""Tests of the my_model model.')
    assert "def test_" not in text


def test_new_passes_bare_on(project, capsys) -> None:
    assert cli.new(project, "my_model", cpp=True, bare=True) == 0
    assert (project / "src/models/my_model/equations.cpp").is_file()
    assert "not a valid model" in capsys.readouterr().out


def test_every_new_model_gets_its_own_guid(project) -> None:
    create_model(project, "first")
    create_model(project, "second")

    def guid(model: str) -> str:
        text = (project / "src" / "models" / model / "variables.c").read_text()
        return re.search(r'\.guid\s*=\s*"(\{[0-9a-f-]{36}\})"', text).group(1)

    assert guid("first") != guid("second")


def test_new_refuses_an_existing_model_in_any_case(project) -> None:
    add_model(project, "First_Order")

    assert cli.new(project, "first_order") == 1
    assert [p.name for p in (project / "src" / "models").iterdir()] == ["First_Order"]
    assert not (project / "src" / "models" / "First_Order" / "variables.h").exists()


def test_new_refuses_an_invalid_name(project) -> None:
    assert cli.new(project, "my-model") == 1
    assert list((project / "src" / "models").iterdir()) == []


# rm


def test_rm_removes_every_trace_of_the_model(project, monkeypatch) -> None:
    create_model(project, "my_model")
    (project / "build" / "my_model").mkdir(parents=True)
    (project / "build" / "my_model.fmu").write_text("")
    answer(monkeypatch, "y")

    assert cli.remove(project, "my_model") == 0
    assert cli.model_traces(project, "my_model") == []


def test_rm_keeps_everything_if_not_confirmed(project, monkeypatch) -> None:
    create_model(project, "my_model")
    answer(monkeypatch, "n")

    assert cli.remove(project, "my_model") == 1
    assert cli.available_models(project) == ["my_model"]


@pytest.mark.parametrize("name", ["..", "../src", "a/b", "a\\b", ""])
def test_rm_refuses_paths(project, name) -> None:
    assert cli.remove(project, name) == 1


def test_references_are_found_as_whole_words(project) -> None:
    (project / "docs").mkdir()
    (project / "docs" / "uses.md").write_text("see my_model for details")
    (project / "docs" / "other.md").write_text("my_model_2 is another model")
    (project / "build").mkdir()
    (project / "build" / "log.txt").write_text("my_model")  # build/ is skipped

    assert cli.references_to(project, "my_model", excluded=[]) == [
        project / "docs" / "uses.md"
    ]
