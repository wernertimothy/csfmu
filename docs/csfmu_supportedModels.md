# Supported models

An FMU built with csfmu contains a continuous-time model that is described by ordinary differential equations. The FMU integrates the equations itself, so the integration tool only exchanges inputs and outputs at the communication points.

This page describes which models can be written today and lists the demo models of this repository, each of which showcases one feature.

## System class

A model is described by a state equation and an output equation:

```
dx/dt = f(t, x, u)      x: vector of doubles
    y = h(t, x, u)
```

where **t** is the time, **x** are the states, **u** are the inputs and **y** are the outputs. Parameters enter both equations.

- `f` and `h` may be nonlinear.
- A model may have no states at all. It is then a static map `y = h(t, u)`, such as a lookup table.
- `f` and `h` may depend explicitly on time. `model->time` is the time of the integrator stage inside `f` and the communication point inside `h`.
- `h` may use the inputs directly (direct feedthrough). You declare which inputs an output depends on with `.dependsOn`, and the `modelDescription.xml` publishes it.
- Besides the outputs, `h` may compute **local variables**: values that can be read from outside but are no outputs, such as a heat flow or an error term.
- The initial states are the start values, or the model computes them in its initialization, e.g. a steady state. What the integration tool reads in initialization mode is already initialized, so connected FMUs can find a consistent start together. The initialization can also read **data files** that the FMU carries along (its resources), such as a table. Parameters that follow from other parameters (such as a time constant `R*C`) can be published as **calculated parameters**.

How to write the two equations is explained in [getting started](csfmu_gettingStarted.md#equationsc-or-equationscpp).

## Variables

- **Parameters**, **inputs**, **outputs** and **local variables** can be of type `double`, `int` or `bool`.
- An `int` variable can be published as an **enumeration**: a named set of values (items), of which only the items can be set.
- Parameters can also be **strings** (`char name[MODEL_STRING_SIZE]`), e.g. a file name. A string holds up to 255 bytes of UTF-8 text; a longer text is refused, never cut. String parameters are fixed or tunable like any other parameter and are part of the FMU state.
- Local variables are computed by the output equation and published read-only (`causality="local"`). They are always consistent with the other variables, like outputs.
- Parameters are tunable (they can be changed between simulation steps) or fixed (`.variability = VAR_FIXED`, they can only be set before the simulation starts).
- Calculated parameters are computed from the parameters and published read-only (`causality="calculatedParameter"`). They are consistent with the parameters in every state of the FMU, also during the initialization.
- A state whose start value the model may replace during the initialization is published with `initial="approx"`; it can be set before the initialization starts.
- **States** are of type `double`, because the integrators work on a vector of doubles.

## Integration

- The models are integrated with fixed-step explicit methods: `FORWARD_EULER` and `RUNGE_KUTTA_4`, selected with `.integrator` in the model definition (see `include/integrator.h`). They suit non-stiff systems. A model can also bring its own method, for example a Runge-Kutta method given by its Butcher tableau (see [getting started](csfmu_gettingStarted.md#how-to-add-your-own-integrator)).
- Every communication step is split into equal sub-steps that are not larger than the model's `maxStepSize`. You choose `maxStepSize` small enough for your model to integrate stably, and the integration tool can use any communication step size.
- Results depend slightly on the communication step size, because each communication step is divided into its own sub-steps. If every communication step is an integer multiple of `maxStepSize`, the sub-step grid is the same however the master groups its steps (up to rounding).

## Interaction with the integration tool

- Inputs are held constant over a communication step (zero-order hold).
- Outputs always reflect the inputs set last, also before a step and during initialization.
- The communication step size can change from step to step.
- The FMU state can be saved, restored and serialized, e.g. to repeat a step.
- The FMU finds its data files wherever the integration tool unzips it, also below a path with spaces or non-ASCII characters.
- If a state, a Real output or a Real local variable becomes NaN or infinite during a step, the FMU discards the step: it returns `fmi2Discard` and is rolled back to the start of the step, so the integration tool can go back to a saved FMU state and continue.

## Demo models

The repository ships demo models, each of which showcases a certain feature. Use them as worked examples for your own model. Every model has its own tests under `tests/models/<model>/`.

| Model | Showcases |
|-------|-----------|
| [`first_order`](../src/models/first_order/) | The simplest model: a linear first-order lag, integrated with explicit Euler. It is the default model of the build and the tests |
| [`mass_spring_damper`](../src/models/mass_spring_damper/) | A second-order system written as two first-order states. Outputs and states use the same field names, so the states are published under their own names with `STATE_AS` |
| [`pendulum_on_cart`](../src/models/pendulum_on_cart/) | A nonlinear fourth-order system whose equations are written in C++ (`equations.cpp`) |
| [`typed_demo`](../src/models/typed_demo/) | `int` and `bool` parameters, inputs and outputs next to `double` ones, and a fixed parameter |
| [`forced_pt1`](../src/models/forced_pt1/) | Explicit time dependence in both equations (`model->time`). The integrator evaluates every stage at its own time |
| [`pi_controller`](../src/models/pi_controller/) | Direct feedthrough: its outputs react to an input as soon as it is set, without a simulation step (`.dependsOn`) |
| [`fault_injection`](../src/models/fault_injection/) | A model without inputs, and a numerical failure on request: the FMU discards the step and rolls back (`fmi2Discard`) |
| [`thermal_mass`](../src/models/thermal_mass/) | A local variable (`Q_loss`, the heat loss to the ambient) that can be read but not set; a calculated parameter (`tau = R*C`); an initialization that can start in steady state (`init_mode`, an enumeration), with a state published as `initial="approx"` |
| [`lookup_table_1d`](../src/models/lookup_table_1d/) | A model without states: a static map that interpolates its input in a table (clamped at both ends), with direct feedthrough. The table is a data file of the FMU (`resources/table.csv`), read by the initialization; its name is a String parameter (`table_file`). Before the initialization the output is NaN |

When the template learns a new kind of model, this page and the list of demo models are extended.
