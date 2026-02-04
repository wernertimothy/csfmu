# fmuTemplate

**A template for synchronous Co-Simulation FMUs written in C.**

---

fmuTemplate provides a minimal, self-contained implementation of the FMI Co-Simulation interface in C, designed for a fast and easy integration of your custom models.

Key features:
- **Plug-and-play model interface**: Add your model without modifying the FMU code
- **FMU-agnostic design**: Models can be tested and simulated independently
- **Compile-time model selection**: Choose your model via CMake build options
- **Extensible architecture**: It's easy to add new models and integrators or extend the fmi implementation.

## Quickstart

Let's build your first FMU.

### Prerequisites
- CMake (3.10+)
- C compiler (GCC, Clang, or MSVC)
- Python 3.7+ with pip (for testing)

### Build Your First FMU

**Linux / macOS:**
```bash
git clone https://github.com/wernertimothy/fmuTemplate.git
cd fmuTemplate
mkdir -p build && cd build
cmake -DMODEL=first_order ..
cmake --build .
```

This creates `fmuTemplate.fmu` in the `build/` directory.

### Test It

Navigate back to the repository root.
```bash
cd ..
```

from there

**Linux / macOS:**
```bash
python3 -m venv .venv
source .venv/bin/activate
pip3 install -r requirements.txt
pytest
```

### What just Happened?
You've compiled a **first-order lag system** (PT1) into a standard-compliant FMU that can be:
- Simulated standalone with FMPy
- Imported into tools like Simulink, Dymola, or OpenModelica
- Used in co-simulation environments
And tesed it's build and runtime sanity with FMPy

## Getting Started

Take a look at [getting started][], to learn about the project architecture and what it takes to implement your own model as an FMU.

## Co-Simulation FMUs

You want to simulate a model?
Your model is a unit of a coupled system?
The units potentially all come from different places speaking different languages?

FMU is the way to go.

Since you have found your way to this project, you are probably already familiar with what an FMU is.
If not, go here to read more about [Co-Sim FMUs][].

## API Reference

To get an overview over the implemented fmi functions, take a look at the reference for [fmi2][] or [fmi3][].

## Acknowledgements

This project was a way for me to learn a bit about FMUs and C programming.

A lot of it is inspired by [Modelica's Reference-FMUs](https://github.com/modelica/Reference-FMUs).
Make sure to pay their page a visit, if you haven't done so already.

And although most of the code was written by a real human beeing, it was heavily supervised by [Claude Code](https://code.claude.com/docs/en/overview), which accelerated the learning curve regarding both FMU and C code significantly. 

[getting started]: docs/fmuTemplate_gettingStarted.md
[Co-Sim FMUs]: docs/fmuTemplate_coSimFMUs.md
[fmi2]: docs/fmuTemplate_fmi2Reference.md
[fmi3]: docs/fmuTemplate_fmi3Reference.md