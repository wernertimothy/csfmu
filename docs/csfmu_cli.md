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

### `new <model>`

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

The following files are created:

  | File | Content |
  |------|---------|
  | `src/models/<model>/variables.h` | The structs `Params`, `Inputs`, `Outputs`, `States` and the declarations of the equations |
  | `src/models/<model>/variables.c` | The variable table and the model definition, with a freshly generated GUID |
  | `src/models/<model>/equations.c` | The state and output equations |
  | `test/models/<model>/__init__.py` | Makes the tests a Python package |
  | `test/models/<model>/test_fmi_compliance.py` | FMI compliance checks of the FMU with FMPy |

The model builds and passes all tests as it is. Replace the integrator with your own equations; [getting started][] explains the three model files in detail.
The test file only checks FMI compliance. It is the place for your own model-specific tests, which are skipped automatically when another model is tested.

`new` only creates files. It does not build or test anything; it tells you the next steps instead:

```bash
uv run csfmu build my_model
uv run csfmu test my_model
```

**Naming rules.** The model name ends up as folder name, CMake target, FMI `modelIdentifier`, file name of the FMU and Python package name of the tests. It therefore has to be valid for all of them:

* only lowercase letters `a-z`, digits `0-9` and underscores `_`
* starts with a letter, no double underscore `__`, no trailing underscore
* at most 64 characters
* not a C, C++ or Python keyword (e.g. `int`, `class`, `import`)
* not a name reserved by CMake or this project (e.g. `all`, `test`, `install`, `unity`, `model_core`) or by Windows (e.g. `con`, `nul`)

In short, use `snake_case`, like `first_order` or `lorenz_attractor`.
Uppercase letters are not allowed because macOS and Windows do not distinguish `My_Model` from `my_model` in file names.

If `src/models/<model>/` or `test/models/<model>/` already exists, `new` stops with an error and changes nothing.

To undo `new`, use `rm`.

### `rm <model>`

Removes a model and all of its files.

```bash
uv run csfmu rm my_model
```

The following files and folders are removed, as far as they exist:

  | Path | Content |
  |------|---------|
  | `src/models/<model>/` | The model |
  | `test/models/<model>/` | Its tests |
  | `examples/example_<model>.py` | Its Python example |
  | `build/<model>/` | Its build folder |
  | `build/<model>.fmu` | Its FMU |

Before anything is removed, `rm` shows the list and asks for confirmation. Enter (or `y`) removes the model; `n` aborts.
If there is no one to answer, for example in a script, nothing is removed.

```
$ uv run csfmu rm mass_spring_damper
This removes model 'mass_spring_damper':
  src/models/mass_spring_damper/
  test/models/mass_spring_damper/
  examples/example_mass_spring_damper.py
These files still mention 'mass_spring_damper' and are not changed:
  .github/workflows/ci.yml
  docs/csfmu_gettingStarted.md
  test/integration/test_two_fmus.py
Remove model 'mass_spring_damper'? [Y/n]
```

Other files of the project may mention the model as well, like the CI workflow or the docs above. `rm` never changes them, because they are shared with other models. It lists them instead, so you can update them by hand.
Keep in mind that `first_order` is the default model of the `CMakeLists.txt` and of the tests (`test/conftest.py`).

`rm` also removes leftovers of a model that is no longer complete, e.g. a `test/models/<model>/` whose model folder was deleted by hand.

### `build [<model>]`

Builds the FMU of a model.

```bash
uv run csfmu build first_order
```

The model is configured and built in `build/<model>/`, and the packaged FMU is placed at `build/<model>.fmu`.
Each model has its own build folder, so switching between models doesn't trigger a full rebuild.

This is the same as:

```bash
cmake -S . -B build/first_order -DMODEL=first_order -DFMU_OUTPUT_DIR=build
cmake --build build/first_order
```

### `test [<model>] [-- <pytest args>]`

Tests a model.

```bash
uv run csfmu test first_order
```

1. If `build/<model>.fmu` does not exist yet, the model is built first (like `build`).
2. The C unit tests are run with `ctest` in `build/<model>/`.
3. The pytest suite in `test/` is run against `build/<model>.fmu`. csfmu passes the model to the tests through the environment variables `FMU_MODEL` and `FMU_OUTPUT_DIR`.

csfmu stops at the first step that fails.

Everything after `--` is passed on to pytest, for example to run only some of the tests with verbose output:

```bash
uv run csfmu test my_model -- -k compliance -v
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
