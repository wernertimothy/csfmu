"""Tests of the csfmu command line tool that need neither CMake nor an FMU.

They work on a fake project in a temporary folder and run once, not per
model: uv run pytest tools/tests. That a scaffolded model builds and passes
its tests is checked by the CLI job of the CI.

The master of csfmu simulate runs on a fake FMU that records every call, so
these tests pin the order of the calls and the communication points: two
builds of a model can only be compared bit for bit if the master drives them
the same way. The master on real FMUs is tested per model in
tests/integration/test_simulate.py.
"""

import csv
import math
import re
from pathlib import Path
from types import SimpleNamespace

import pytest
from csfmu import cli
from csfmu.scaffold import camel_case, create_model, validate_name
from csfmu.simulate import (
    InputTable,
    Result,
    SimulationError,
    format_value,
    number_of_steps,
    parse_value,
    read_inputs,
    recorded_variables,
    run,
    write_csv,
)
from fmpy.fmi1 import FMICallException

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


def test_simulate_takes_its_settings_from_the_fmu_by_default() -> None:
    args = cli.parse_args(["simulate", "my_model"])

    assert args.model == "my_model"
    assert (args.input, args.output) == (None, None)
    assert [args.start, args.stop, args.step, args.tolerance] == [None] * 4
    assert args.start_values == []


def test_simulate_collects_repeated_start_values() -> None:
    args = cli.parse_args(
        ["simulate", "my_model", "--set", "k=2", "--set", "name=a=b.csv", "--stop", "1"]
    )

    assert args.start_values == [("k", "2"), ("name", "a=b.csv")]
    assert args.stop == 1.0


def test_simulate_refuses_a_start_value_without_value(capsys) -> None:
    with pytest.raises(SystemExit):
        cli.parse_args(["simulate", "my_model", "--set", "k"])

    assert "NAME=VALUE" in capsys.readouterr().err


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


# simulate: a fake model and FMU for the master


def variable(name, type="Real", causality="local", reference=0, start=None):
    """A model variable as FMPy describes it, with the attributes the master uses."""
    return SimpleNamespace(
        name=name,
        type=type,
        causality=causality,
        valueReference=reference,
        start=start,
    )


# A model like the scaffold template: x integrates k*u, y = x
K = variable("k", causality="parameter", reference=0)
U = variable("u", causality="input", reference=1)
Y = variable("y", causality="output", reference=2)
X = variable("x", reference=3)
N = variable("n", type="Integer", causality="output", reference=4)
ON = variable("on", type="Boolean", causality="input", reference=5)
NAME = variable("name", type="String", causality="parameter", reference=6)
MODEL = SimpleNamespace(modelVariables=[K, U, Y, X, N, ON, NAME])


class FakeFMU:
    """Records the calls of the master and integrates dx/dt = k*u exactly.

    n counts the steps. doStep fails with fmi2Discard from t = fail_at on.
    """

    def __init__(self, fail_at=None, fail_in=None):
        self.values = {0: 1.0, 1: 0.0, 2: 0.0, 3: 0.0, 4: 0, 5: 0}
        self.calls = []
        self.fail_at = fail_at
        self.fail_in = fail_in

    def call(self, name, *args):
        self.calls.append((name, *args))
        if name == self.fail_in:
            raise FMICallException(
                function=f"fmi2{name[0].upper()}{name[1:]}", status=3
            )

    def setupExperiment(self, tolerance=None, startTime=0.0, stopTime=None):
        self.call("setupExperiment", tolerance, startTime, stopTime)

    def enterInitializationMode(self):
        self.call("enterInitializationMode")

    def exitInitializationMode(self):
        self.call("exitInitializationMode")

    def terminate(self):
        self.call("terminate")

    def doStep(self, currentCommunicationPoint, communicationStepSize):
        self.call("doStep", currentCommunicationPoint, communicationStepSize)
        if self.fail_at is not None and currentCommunicationPoint >= self.fail_at:
            raise FMICallException(function="fmi2DoStep", status=2)
        self.values[3] += self.values[0] * self.values[1] * communicationStepSize
        self.values[2] = self.values[3]
        self.values[4] += 1

    def _set(self, name, references, values):
        self.call(name, list(references), list(values))
        self.values.update(zip(references, values))

    def _get(self, name, references):
        self.call(name, list(references))
        return [self.values[reference] for reference in references]

    def setReal(self, references, values):
        self._set("setReal", references, values)

    def setInteger(self, references, values):
        self._set("setInteger", references, values)

    def setBoolean(self, references, values):
        self._set("setBoolean", references, values)

    def getReal(self, references):
        return self._get("getReal", references)

    def getInteger(self, references):
        return self._get("getInteger", references)

    def getBoolean(self, references):
        return self._get("getBoolean", references)


def names(calls) -> list[str]:
    return [call[0] for call in calls]


def write_file(path, text: str):
    path.write_text(text, encoding="utf-8")
    return path


# simulate: communication points


def test_steps_reach_the_stop_time() -> None:
    assert number_of_steps(0.0, 10.0, 0.01) == 1000
    assert number_of_steps(0.0, 0.3, 0.1) == 3  # 3*0.1 is 0.30000000000000004
    assert number_of_steps(1.5, 2.0, 0.25) == 2


@pytest.mark.parametrize(
    "start, stop, step",
    [
        (0.0, 0.055, 0.01),  # not on the grid
        (0.0, 0.0, 0.1),  # no step at all
        (1.0, 0.0, 0.1),  # backwards
        (0.0, 1.0, 0.0),
        (0.0, 1.0, -0.1),
        (0.0, math.inf, 0.1),
        (math.nan, 1.0, 0.1),
    ],
)
def test_bad_grid_is_refused(start, stop, step) -> None:
    with pytest.raises(SimulationError):
        number_of_steps(start, stop, step)


# simulate: values


@pytest.mark.parametrize(
    "value",
    [
        0.1 + 0.2,
        1 / 3,
        5e-324,
        1.7976931348623157e308,
        -0.0,
        1e-5,
        math.pi,
        math.inf,
        -math.inf,
    ],
)
def test_real_values_are_written_exactly(value) -> None:
    text = format_value(Y, value)

    assert float(text) == value
    assert math.copysign(1.0, float(text)) == math.copysign(1.0, value)


def test_nan_is_written_as_nan() -> None:
    assert math.isnan(float(format_value(Y, math.nan)))


def test_integer_and_boolean_values_are_written_as_numbers() -> None:
    assert format_value(N, 42) == "42"
    assert format_value(N, -3) == "-3"
    assert format_value(ON, True) == "1"
    assert format_value(ON, False) == "0"


def test_texts_are_parsed_by_type() -> None:
    assert parse_value(Y, " 1e-3 ") == 1e-3
    assert parse_value(N, "7") == 7
    assert parse_value(ON, "true") is True
    assert parse_value(ON, "FALSE") is False
    assert parse_value(ON, "1") is True
    assert parse_value(NAME, "table.csv") == "table.csv"


@pytest.mark.parametrize("var, text", [(Y, "one"), (N, "1.5"), (ON, "yes"), (Y, "")])
def test_bad_texts_are_refused(var, text) -> None:
    with pytest.raises(SimulationError, match=var.name):
        parse_value(var, text)


def test_parameters_and_strings_are_not_recorded() -> None:
    assert recorded_variables(MODEL) == [U, Y, X, N, ON]


# simulate: input file


def test_input_file_is_read(tmp_path) -> None:
    path = write_file(tmp_path / "in.csv", "time, u, on\n0, 1.5, true\n\n0.25,-2,0\n")

    table = read_inputs(path, MODEL)

    assert table.variables == [U, ON]
    assert table.times == [0.0, 0.25]
    assert table.rows == [[1.5, True], [-2.0, False]]


@pytest.mark.parametrize(
    "text, message",
    [
        ("", "empty"),
        ("t,u\n0,1\n", "first column"),
        ("time,v\n0,1\n", r"'v' .* \(inputs: u, on\)"),
        ("time,y\n0,1\n", "'y' .* not an input"),
        ("time,u,u\n0,1,2\n", "two columns"),
        ("time,u\n0,1,2\n", "line 2"),
        ("time,u\n0,1\nlater,2\n", "line 3 of the input file: 'later' is not a time"),
        (
            "time,u\n1,1\n0.5,2\n",
            "line 3 of the input file: the time 0.5 is before 1.0",
        ),
        ("time,u\n0,high\n", "line 2 of the input file: 'high' is not a value of 'u'"),
    ],
)
def test_bad_input_file_is_refused(tmp_path, text, message) -> None:
    path = write_file(tmp_path / "in.csv", text)

    with pytest.raises(SimulationError, match=message):
        read_inputs(path, MODEL)


def test_missing_input_file_is_refused(tmp_path) -> None:
    with pytest.raises(SimulationError, match="cannot read"):
        read_inputs(tmp_path / "missing.csv", MODEL)


def test_inputs_are_held_from_their_row_on() -> None:
    table = InputTable([U], [0.5, 1.0, 1.0, 2.0], [[1.0], [2.0], [3.0], [4.0]])

    assert table.active_row(0.0, 0.0) is None  # before the first row
    assert table.active_row(0.5, 0.0) == 0  # exactly on a row
    assert table.active_row(0.75, 0.0) == 0  # between rows
    assert table.active_row(1.0, 0.0) == 2  # same time twice: the last row wins
    assert table.active_row(5.0, 0.0) == 3  # after the last row


def test_a_row_on_a_communication_point_holds_there_despite_rounding() -> None:
    table = InputTable([U], [0.3], [[1.0]])

    assert table.active_row(3 * 0.1, 1e-10) == 0  # 0.30000000000000004
    assert table.active_row(0.29999999999999993, 1e-10) == 0
    assert table.active_row(0.2999, 1e-10) is None


# simulate: master


def test_master_calls_the_fmu_in_a_fixed_order() -> None:
    fmu = FakeFMU()
    inputs = InputTable([U], [0.0, 0.25], [[1.0], [2.0]])

    run(fmu, [Y], 0.0, 0.1, 3, inputs=inputs, start_values=[(K, 2.0)], tolerance=1e-8)

    assert fmu.calls == [
        ("setupExperiment", 1e-8, 0.0, 0.30000000000000004),
        ("setReal", [0], [2.0]),  # start value
        ("setReal", [1], [1.0]),  # input of t = 0
        ("enterInitializationMode",),
        ("exitInitializationMode",),
        ("getReal", [2]),
        ("doStep", 0.0, 0.1),
        ("getReal", [2]),
        ("doStep", 0.1, 0.1),
        ("getReal", [2]),
        ("doStep", 0.2, 0.1),
        ("setReal", [1], [2.0]),  # the row of 0.25 holds from t = 0.3 on
        ("getReal", [2]),
        ("terminate",),
    ]


def test_communication_points_are_not_accumulated() -> None:
    fmu = FakeFMU()

    result = run(fmu, [X], 0.0, 0.1, 10)

    expected = [k * 0.1 for k in range(11)]
    assert result.times == expected
    assert [call[1] for call in fmu.calls if call[0] == "doStep"] == expected[:-1]
    # What a master that adds up the step size would reach instead (sum()
    # would not show it: it adds floats with compensation)
    accumulated = 0.0
    for _ in range(10):
        accumulated += 0.1
    assert result.times[-1] != accumulated


def test_rows_hold_the_values_after_the_inputs_of_their_time() -> None:
    fmu = FakeFMU()
    inputs = InputTable([U], [0.2], [[1.0]])

    result = run(fmu, [U, X, N, ON], 0.0, 0.1, 4, inputs=inputs)

    assert result.error is None
    assert result.rows == [
        [0.0, 0.0, 0, False],
        [0.0, 0.0, 1, False],
        [1.0, 0.0, 2, False],
        [1.0, 0.1, 3, False],
        [1.0, 0.2, 4, False],
    ]


def test_failed_step_keeps_the_rows_up_to_its_start() -> None:
    fmu = FakeFMU(fail_at=0.2)

    result = run(fmu, [N], 0.0, 0.1, 5)

    assert result.times == [0.0, 0.1, 0.2]
    assert result.rows == [[0], [1], [2]]
    assert "step from t = 0.2" in result.error
    assert "discard" in result.error
    assert "terminate" not in names(fmu.calls)


def test_failed_initialization_records_nothing() -> None:
    fmu = FakeFMU(fail_in="exitInitializationMode")

    result = run(fmu, [N], 0.0, 0.1, 5)

    assert result.rows == []
    assert result.error.startswith("initialization: fmi2ExitInitializationMode failed")
    assert "doStep" not in names(fmu.calls)


# simulate: result file


def test_result_file_reads_back_exactly(tmp_path) -> None:
    result = Result(
        [Y, N, ON],
        [0.0, 0.1, 0.30000000000000004],
        [[0.1 + 0.2, 1, True], [-0.0, -2, False], [math.inf, 3, True]],
    )
    path = tmp_path / "result.csv"

    write_csv(path, result)

    data = path.read_bytes()
    assert b"\r" not in data
    lines = list(csv.reader(data.decode("utf-8").splitlines()))
    assert lines[0] == ["time", "y", "n", "on"]
    assert [float(line[0]) for line in lines[1:]] == result.times
    assert [
        [parse_value(v, text) for v, text in zip(result.variables, line[1:])]
        for line in lines[1:]
    ] == result.rows
