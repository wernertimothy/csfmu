# The csfmu project manager

`csfmu` is a small command line tool that takes care of the everyday tasks of this project: creating, building and testing models.
It is a thin wrapper around CMake, CTest and pytest, so everything it does can also be done by hand (see the commands below).

It is installed into the project's virtual environment by `uv sync` and is called through `uv`:

```bash
uv run csfmu <command> [<model>]
```

`uv run csfmu --help` lists all commands, and `uv run csfmu <command> --help` shows the options of a single command.

csfmu can be called from anywhere inside the project. It looks for the project root in the current directory and its parents: the first directory that contains a `CMakeLists.txt` and a `src/models/` folder.

## Models

A model is a folder under `src/models/` that contains a `variables.c` and an `equations.c` or `equations.cpp`.
This is the same check the `CMakeLists.txt` performs, so every model csfmu knows about can be built.

The model name is the name of that folder. It is also the name of the FMU (`<model>.fmu`) and its `modelIdentifier`.

`build` and `test` take the model name as an optional argument.
If you omit it, or the model does not exist, csfmu lists the available models and lets you pick one:

```
$ uv run csfmu build
Available models:
  [1] first_order
  [2] mass_spring_damper
  [3] pendulum_on_cart
  [4] typed_demo
Select a model [1-4] or q to quit:
```

## Commands

### `new <model> [--cpp] [--bare]`

Creates a new model from a template.

```bash
uv run csfmu new my_model
```

The template is the simplest dynamical system there is, an integrator with gain:

  | Variable | Role | Start | Description |
  |----------|------|-------|-------------|
  | `k` | parameter | 1.0 | gain |
  | `u` | input | 0.0 | input signal |
  | `x` | state | 0.0 | state of the integrator |
  | `y` | output | | equal to the state |

with **dx/dt = k·u** and **y = x**.

With `--cpp` the equations are written in C++: the same integrator goes into `equations.cpp` instead of `equations.c`.

```bash
uv run csfmu new my_model --cpp
```

`variables.c` stays a C file, also for a C++ model, because the variable table needs C. [C++ models][] in getting started explains what differs in C++.

The following files are created:

  | File | Content |
  |------|---------|
  | `src/models/<model>/variables.h` | The structs `Params`, `Inputs`, `Outputs`, `States` and the declarations of the equations |
  | `src/models/<model>/variables.c` | The variable table and the model definition, with a freshly generated GUID |
  | `src/models/<model>/equations.c` (`equations.cpp` with `--cpp`) | The state and output equations |
  | `tests/models/<model>/__init__.py` | Makes the tests a Python package |
  | `tests/models/<model>/test_<model>.py` | A starter test that checks the integrator against its exact solution |

The model builds and passes all tests as it is. Replace the integrator with your own equations; [getting started][] explains the three model files in detail.
The FMI behavior of every model is checked by the tests in `tests/integration/`. `tests/models/<model>/` is the place for the tests of your own equations; they are only collected when this model is tested.

`new` only creates files. It does not build or test anything; it tells you the next steps instead:

```bash
uv run csfmu build my_model
uv run csfmu test my_model
```

**Bare models.** With `--bare` the files are an empty skeleton instead of the integrator, to be filled in with your own model:

```bash
uv run csfmu new my_model --bare
```

  | File | Content |
  |------|---------|
  | `src/models/<model>/variables.h` | All six structs, empty: `Params`, `Inputs`, `Outputs`, `Locals`, `States` and `Internals`, and the declarations of the four functions |
  | `src/models/<model>/variables.c` | The model definition with every field written out and a freshly generated GUID, but no variable table and no unit table yet |
  | `src/models/<model>/equations.c` (`equations.cpp` with `--cpp`) | The state equation, the output equation, the calculated parameters and the initialization, all empty. Comments in each function show how to view the storage blocks it may use |
  | `tests/models/<model>/test_<model>.py` | Only a docstring, no tests yet |

Implement what your model needs and delete what it does not need: a struct together with its line in `MODEL_DEFINITION`, and the calculated parameters or the initialization together with their declaration and their line in `MODEL_DEFINITION`.
The state and output equations, the integrator and `maxStepSize` are required, also for a model without states (see `lookup_table_1d`). [getting started][] explains each part.

A bare model does not build into a working FMU as it is, it is only a starting point.

`--bare` and `--cpp` can be combined: `uv run csfmu new my_model --bare --cpp` creates the empty skeleton with `equations.cpp`.

**Naming rules.** The model name ends up as folder name, CMake target, FMI `modelIdentifier`, file name of the FMU and Python package name of the tests. It therefore has to be valid for all of them:

* only letters `a-z` and `A-Z`, digits `0-9` and underscores `_`
* starts with a letter, no double underscore `__`, no trailing underscore
* at most 64 characters
* not a C, C++ or Python keyword (e.g. `int`, `class`, `import`)
* not a name reserved by CMake or this project (e.g. `all`, `test`, `install`, `ALL_BUILD`, `unity`, `model_core`) or by Windows (e.g. `con`, `nul`), in any case

For example `first_order`, `lorenz_attractor` or `PID_controller`.

If `src/models/<model>/` or `tests/models/<model>/` already exists, `new` stops with an error and changes nothing.
Names that only differ in case count as the same model (`My_Model` already exists if `my_model` does), on every platform: macOS and Windows do not distinguish them in file names, so the project would break when checked out there.

To undo `new`, use `rm`.

### `rm <model>`

Removes a model and all of its files.

```bash
uv run csfmu rm my_model
```

The model name must be given exactly, including its case: `rm First_Order` does not remove `first_order`.

The following files and folders are removed, as far as they exist:

  | Path | Content |
  |------|---------|
  | `src/models/<model>/` | The model |
  | `tests/models/<model>/` | Its tests |
  | `examples/example_<model>.py` | Its Python example |
  | `build/<model>/` | Its build folder |
  | `build/<model>.fmu` | Its FMU |

Before anything is removed, `rm` shows the list and asks for confirmation. Enter (or `y`) removes the model; `n` aborts.
If there is no one to answer, for example in a script, nothing is removed.

```
$ uv run csfmu rm mass_spring_damper
This removes model 'mass_spring_damper':
  src/models/mass_spring_damper/
  tests/models/mass_spring_damper/
  examples/example_mass_spring_damper.py
These files still mention 'mass_spring_damper' and are not changed:
  .github/workflows/ci.yml
  docs/csfmu_gettingStarted.md
  tests/integration/test_two_fmus.py
Remove model 'mass_spring_damper'? [Y/n]
```

Other files of the project may mention the model as well, like the CI workflow or the docs above. `rm` never changes them, because they are shared with other models. It lists them instead, so you can update them by hand.
Keep in mind that `first_order` is the default model of the `CMakeLists.txt` and of the tests (`tests/conftest.py`).

`rm` also removes leftovers of a model that is no longer complete, e.g. a `tests/models/<model>/` whose model folder was deleted by hand.

### `build [<model>] [-o <dir>]`

Builds the FMU of a model.

```bash
uv run csfmu build first_order
```

The model is configured and built in `build/<model>/`, and the packaged FMU is placed at `build/<model>.fmu`.
Each model has its own build folder, so switching between models doesn't trigger a full rebuild.

With `-o <dir>` (or `--output-dir <dir>`) the FMU is additionally copied to a folder of your choice, which is created if it doesn't exist.
A relative path is relative to the directory you call csfmu from.

```bash
uv run csfmu build first_order -o fmus
```

The FMU is still built in `build/` first and then copied, also when the model is already up to date.
`test` and `rm` only know `build/<model>.fmu`; copies made with `-o` are yours, `rm` does not remove them.

This is the same as:

```bash
cmake -S . -B build/first_order -DMODEL=first_order -DFMU_OUTPUT_DIR=build
cmake --build build/first_order
```

where `-DFMU_OUTPUT_DIR` sets the folder the FMU is placed in.

### `test [<model>] [-- <pytest args>]`

Tests a model.

```bash
uv run csfmu test first_order
```

1. If `build/<model>.fmu` does not exist yet, the model is built first (like `build`).
2. The C unit tests are run with `ctest` in `build/<model>/`. This includes the model's own C tests: if `tests/models/<model>/CMakeLists.txt` exists, they are built together with that model.
3. The pytest suite in `tests/` is run against `build/<model>.fmu`. csfmu passes the model to the tests through the environment variables `FMU_MODEL` and `FMU_OUTPUT_DIR`.

csfmu stops at the first step that fails.

Everything after `--` is passed on to pytest, for example to run only some of the tests with verbose output:

```bash
uv run csfmu test my_model -- -k fmu_state -v
```

Note that `test` does not rebuild an existing FMU. After changing a model, run `build` first, or `clean` the build folder.

### `list`

Lists all models, one per line.

```bash
uv run csfmu list
```

The output is meant to be used by scripts as well; the CI, for example, loops over it to build and test every model.

### `clean`

Removes the `build/` folder, including all build folders and FMUs.

```bash
uv run csfmu clean
```

[getting started]: csfmu_gettingStarted.md
[C++ models]: csfmu_gettingStarted.md#c-models
