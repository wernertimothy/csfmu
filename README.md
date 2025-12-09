# fmuTemplate

A template for Co-Simulation FMUs written in C.

You want to simulate a model?
Your model is a unit of a coupled system?
The units potentially all come frome different places speaking different languages?

FMU is the way to go.

## Co-Simulation FMUs

Since you have found your way to this project, you are probably already familiar with what a FMU is.
If not, go here to read more about [Co-Sim FMUs][].

## Getting Started

This is the best place for [getting started][].

## API Reference

To get an overview over the implemented fmi functions, take a look at the reference for [fmi2][] or [fmi3][]

## Usage

## key ideas
* every model has inputs, outputs, states and parameter
* core model is fmu agnosctic
* FMU CoSimualtion adapter connects fmu API with model

## assumptions
* synchornous
* fixed step size

[getting started]: docs/fmuTemplate_gettingStarted.md
[Co-Sim FMUs]: docs/fmuTemplate_coSimFMUs.md
[fmi2]: docs/fmuTemplate_fmi2Reference.md
[fmi3]: docs/fmuTemplate_fmi3Reference.md