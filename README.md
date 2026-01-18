# fmuTemplate

**A template for synchronous Co-Simulation FMUs written in C.**

You want to simulate a model?
Your model is a unit of a coupled system?
The units potentially all come frome different places speaking different languages?

FMU is the way to go.

This project implements the minimum required interface for synchronous Co-Simulation FMUs using the fmi2 (fmi3 will follow soon) in a single source file.
It also provides a plug-and-play interface for your own model implementation.
The model is completely FMU agnostic to provide as much flexibility as possible.
The model is selected at compile time using a CMake build option, making the architecture a highly generic template and easy to extend as required.

## Getting Started

Take a look at [getting started][], to learn about the project artchitecture and what it takes to implement your own model and make it an FMU.

## Co-Simulation FMUs

Since you have found your way to this project, you are probably already familiar with what an FMU is.
If not, go here to read more about [Co-Sim FMUs][].

## API Reference

To get an overview over the implemented fmi functions, take a look at the reference for [fmi2][] or [fmi3][]

## Acknowledgements

This project was a way for me to learn a bit about C programming.

A lot of it is inspired by [Modellica's Reference-FMUs](https://github.com/modelica/Reference-FMUs).
Make sure to pay their page a visit if you haven't done so already.

And albeit most of the code was written by me (a real human), it was heavily supervised by [Claude Code](https://code.claude.com/docs/en/overview), which accelerated the learning curve regarding both FMU and C significantly. 

[getting started]: docs/fmuTemplate_gettingStarted.md
[Co-Sim FMUs]: docs/fmuTemplate_coSimFMUs.md
[fmi2]: docs/fmuTemplate_fmi2Reference.md
[fmi3]: docs/fmuTemplate_fmi3Reference.md