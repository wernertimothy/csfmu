# csfmu

**A template for synchronous Co-Simulation FMUs written in C.**

---

csfmu provides a minimal, self-contained implementation of the FMI Co-Simulation interface in C, designed for a fast and easy integration of your custom models.

Key features:
- **Plug-and-play model interface**: Add your model without modifying the FMU code
- **FMU-agnostic design**: Models can be tested and simulated independently
- **Declare variables once**: Parameters, inputs, outputs and states are plain C struct fields (`double`, `int` or `bool`, and strings for parameters), in any order; the `modelDescription.xml` is generated
- **Build-time model selection**: Choose your model via CMake build options
- **Extensible architecture**: It's easy to add new models and integrators or extend the fmi implementation.

## Quickstart

Let's build your first FMU.

### Prerequisites
- CMake (3.21+)
- C compiler (GCC, Clang, or MSVC)
- uv for testing

The quickest way to build and test a model is via the `csfmu` cli:

```bash
git clone https://github.com/wernertimothy/csfmu.git
cd csfmu
uv sync
uv run csfmu build first_order
uv run csfmu test first_order
```

To learn more about the cli, go [here][csfmu cli].

### Build Your First FMU (manually)

Alternatively, build with CMake directly.

**Linux / macOS:**
```bash
git clone https://github.com/wernertimothy/csfmu.git
cd csfmu
mkdir -p build && cd build
cmake -DMODEL=first_order ..
cmake --build .
```

This creates `first_order.fmu` in the `build/` directory. The FMU is named after the selected model. You can change the output directory with `-DFMU_OUTPUT_DIR=<path>`.

### And Test It (manually)

Navigate back to the repository root.
```bash
cd ..
```

from there

**Linux / macOS:**
```bash
uv sync
uv run pytest
```

### What just Happened?
You've compiled a **first-order lag system** (PT1) into a standard-compliant FMU that can be:
- Simulated standalone with FMPy
- Imported into tools like Simulink, Dymola, or OpenModelica
- Used in co-simulation environments

And tested it's build and runtime sanity with FMPy

## Getting Started

Take a look at [getting started][], to learn about the project architecture and what it takes to implement your own model as an FMU.

## Supported Models

Which systems you can turn into an FMU, and which demo model showcases which feature, is described in [supported models][].

## Co-Simulation FMUs

Since you have found your way to this project, you are probably already familiar with what an FMU is.
If not, go here to read more about [Co-Sim FMUs][].

## API Reference

To get an overview over the implemented fmi functions, take a look at the reference for [fmi2][] or [fmi3][].

## Acknowledgements

This project was a way for me to learn a bit about FMUs and C programming.

A lot of it is inspired by [Modelica's Reference-FMUs](https://github.com/modelica/Reference-FMUs).
Make sure to pay their page a visit, if you haven't done so already.

And although most of the code was written by a real human beeing, it was heavily supervised by [Claude Code](https://code.claude.com/docs/en/overview), which accelerated the learning curve regarding both FMU and C code significantly. 

[getting started]: docs/csfmu_gettingStarted.md
[supported models]: docs/csfmu_supportedModels.md
[csfmu cli]: docs/csfmu_cli.md
[Co-Sim FMUs]: docs/csfmu_coSimFMUs.md
[fmi2]: docs/csfmu_fmi2Reference.md
[fmi3]: docs/csfmu_fmi3Reference.md