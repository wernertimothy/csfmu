# fmuTemplate

**A template for synchronous Co-Simulation FMUs written in C.**

You want to simulate a model?
Your model is a unit of a coupled system?
The units potentially all come from different places speaking different languages?

FMU is the way to go.

---

fmuTemplate provides a minimal, self-contained implementation of the FMI 2.0 Co-Simulation interface in C, designed for integrating custom models into
multi-domain simulation environments.

Key features:
- **Plug-and-play model interface**: Add custom models without modifying FMU code
- **FMU-agnostic design**: Models can be tested and simulated independently
- **Compile-time model selection**: Choose models via CMake build options
- **Extensible architecture**: Easy to add new models and integrators

## Getting Started

Take a look at [getting started][], to learn about the project architecture and what it takes to implement your own model as an FMU.

## Co-Simulation FMUs

Since you have found your way to this project, you are probably already familiar with what an FMU is.
If not, go here to read more about [Co-Sim FMUs][].

## API Reference

To get an overview over the implemented fmi functions, take a look at the reference for [fmi2][] or [fmi3][].

## Acknowledgements

This project was a way for me to learn a bit about FMUs and C programming.

A lot of it is inspired by [Modelica's Reference-FMUs](https://github.com/modelica/Reference-FMUs).
Make sure to pay their page a visit, if you haven't done so already.

And although most of the code was written by me (a real human), it was heavily supervised by [Claude Code](https://code.claude.com/docs/en/overview), which accelerated the learning curve regarding both FMU and C significantly. 

[getting started]: docs/fmuTemplate_gettingStarted.md
[Co-Sim FMUs]: docs/fmuTemplate_coSimFMUs.md
[fmi2]: docs/fmuTemplate_fmi2Reference.md
[fmi3]: docs/fmuTemplate_fmi3Reference.md