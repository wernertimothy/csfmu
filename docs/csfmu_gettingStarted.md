# Getting started with csfmu

**Welcome.**

This overview will give you all information necessary to start implementing your own model equations, compile it to an FMU and test it, using this template.

The Project is structured as follows.

## The Folders

  | Directory | Contents |
  |-----------|----------|
  | `docs/` | References and documentation |
  | `examples/` | Example implementations (see note below) |
  | `external/` | Third-party dependencies (FMI headers, Unity) |
  | `include/` | Project header files |
  | `scripts/` | Build and packaging scripts |
  | `src/` | Core FMU and model-interface implementation |
  | `src/models/` | **Model implementations (this is where you will add your model)** |
  | `tests/` | Unit and integration tests |

**Note on examples:** The C examples (`example_model.c`, `example_fmu.c`) are self-contained references for the `first_order` model and only build when `-DMODEL=first_order` is selected. `example_model.c` simulates the model without FMI: it calls `simulateTo()` (`include/simulation.h`), which advances the model exactly as the FMU does in `fmi2DoStep`. Some models also have a Python example script (`examples/example_<model_name>.py`) that demonstrates simulation with fmpy.

## Core principle of csfmu

This template is offering an expandable library of models with minimum restrictive implementation specifications alongside a model independent fmi implementation.

The two sides are kept apart by one idea: **a model describes its variables once, in a plain C table.**

* The **model** knows nothing about FMI. It declares its parameters, inputs, outputs and states as ordinary C structs and lists them in a table (name, start value, unit, ...).
* The **FMI implementation** knows nothing about the model. It finds every variable through that table.
* The **`modelDescription.xml`** is generated from the same table during the build. You never write it by hand, and you never deal with value references.

As a consequence there are no ordering rules: variables can be declared in any order and can be of type `double`, `int` or `bool`, and parameters can also be strings.

Which systems a model can describe, and a demo model for each feature, is listed in [supported models](csfmu_supportedModels.md).

## How to Build the FMU

Run the following to build a specific **model_name**, which needs to match a folder name under `src/models`.
For example if you want to build the **first_order** model and package the fmu to a folder called `fmus` inside the root, run

```bash
rm -rf build
mkdir -p build && cd build
cmake -DMODEL=first_order -DFMU_OUTPUT_DIR=../fmus ..
cmake --build .
```

Or let the project manager do it, which builds in `build/first_order/` and places the FMU at `build/first_order.fmu`:

```bash
uv run csfmu build first_order
```

To run the FMU and record its trajectory as CSV, use `uv run csfmu simulate first_order` (see [simulation](csfmu_simulation.md)).

## How to add your own Model

Start by picking an apropreate **name** for you model.
Let's go with the famous **lorenz_attractor** for the sake of this tutorial.

First, create a new folder called `src/models/lorenz_attractor`, for your model.

Second, add the following files and fill them with life:

| File | Content |
|------|-------------|
| **variables.h** | The variables of your model as C structs, and the declarations of your equations |
| **variables.c** | The table of published variables and the model definition (always a C file) |
| **equations.c** or **equations.cpp** | The state and output equations, that describe your model (C or C++) |
| **resources/** (optional) | Data files of your model, such as tables, which the FMU carries along (see [Resources](#resources)) |

That is all. You do not have to touch any other file of the project: the model is picked up by `-DMODEL=lorenz_attractor`.

**Tip:** `uv run csfmu new lorenz_attractor` creates the folder and all three files for you, filled with a simple integrator that builds and passes the tests right away. Add `--cpp` to get the equations in C++, or `--bare` to get an empty skeleton of every part a model can have, instead of the integrator. See [the project manager](csfmu_cli.md) for details.

The build system auto-detects whether your model uses C or C++ based on the file extension. If `equations.cpp` exists it will be compiled with the C++ compiler; otherwise `equations.c` is used.

Let's take a look at these files one by one in detail.

---

### `variables.h`

This header declares the storage of your model as plain structs. The struct names are fixed, the fields are up to you:

| Struct | Content | Allowed field types |
|--------|---------|---------------------|
| `Params` | Model parameters, and the parameters your model calculates from them (see [Initialization](#initialization)) | `double`, `int`, `bool`, and `char name[MODEL_STRING_SIZE]` for a string (see [String parameters](#string-parameters)) |
| `Inputs` | Input signals | `double`, `int`, `bool` |
| `Outputs` | Output signals | `double`, `int`, `bool` |
| `Locals` | Computed values that can be read from outside, but are no outputs (see [Local variables](#local-variables)) | `double`, `int`, `bool` |
| `States` | Continuous states | `double` only |
| `Internals` | Memory of the model that is not published (see [below](#internals)) | Plain values and fixed-size arrays |

Leave out a struct your model does not need (the Lorenz attractor has no inputs).

```c
#ifndef LORENZ_ATTRACTOR_VARIABLES_H
#define LORENZ_ATTRACTOR_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double sigma;  /* Prandtl number */
    double rho;    /* Rayleigh number */
    double beta;   /* geometric factor */
} Params;

/* outputs */
typedef struct {
    double x;
    double y;
    double z;
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x;
    double y;
    double z;
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void lorenzAttractor(modelInstance* model);

/* output equation */
void lorenzAttractorOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
```

#### Internals

`Internals` holds memory that your model needs but does not publish: the integration tool can neither read nor set it, and it does not appear in the `modelDescription.xml`. Declare the struct in `variables.h` and give its size in the model definition with `.sizeOfInternals = sizeof(Internals)`.

```c
/* internals (not published) */
typedef struct {
    double table[256];  /* plain values and fixed-size arrays */
    int    count;
} Internals;
```

- The internals are zero when the FMU is instantiated and after a reset. They have no start values, since they are not listed in the variable table.
- The internals are part of the FMU state: saving, restoring and serializing the FMU state include them, and a discarded step rolls them back together with the states. They are copied byte by byte, so they hold plain values and fixed-size arrays and no pointers: a pointer would be copied, but not the memory it points to.
- The state and output equations only read `model->internals`. The integrator may call them any number of times per step, so a value written there would depend on the integrator.

---

### `equations.c` (or `equations.cpp`)

In this file you should implement two functions, the models **state equations** and **output equation**.

#### State Equation

This template assumes that your model can be represented as a potentially nonlinear differential equation, such that the derivatives of the states can be computed by a function **dx=f(t,x,u)**, where **dx** are the state derivatives with respect to time **t**, **x** are the states and **u** are the inputs to the model.
**f** is the state equation you have to implement here (see [supported models](csfmu_supportedModels.md) for the class of systems it can describe).
It can have any name.
The signature of the function is
```c
void whateverTheName(modelInstance* model);
```
The model instance gives you access to the storage of your model:

| Member | Type | Content |
|--------|------|---------|
| `model->time` | `double` | the current time **t** |
| `model->relTol` | `double` | the relative tolerance of the experiment (read-only, used by adaptive integrators) |
| `model->params` | `void*` | your `Params` struct |
| `model->inputs` | `void*` | your `Inputs` struct |
| `model->outputs` | `void*` | your `Outputs` struct |
| `model->locals` | `void*` | your `Locals` struct (computed by the output equation, see [Local variables](#local-variables)) |
| `model->internals` | `void*` | your `Internals` struct (read-only in the equations, see [Internals](#internals)) |
| `model->resourcePath` | `char*` | the folder of your data files, ending with `/` (read it in the initialization, see [Resources](#resources)) |
| `model->states` | `double*` | the state vector |
| `model->stateDerivatives` | `double*` | the state derivatives, same layout as the states |

For a time-varying model read **t** from `model->time`, e.g. `double t = model->time;`. The integrator sets it to the time of every stage it evaluates, so you never compute it yourself. The output equation sees the time of the communication point.

The state vector is an array of doubles, because that is what the numerical integrators work on. Your `States` struct consists only of doubles, so it can be used as a view on that array that gives every entry a name. This is the one place where a cast is needed.

Here is an exampe implemenation of the **lorenz_attractor**

```c
#include "variables.h"

void lorenzAttractor(modelInstance* model) {
  const Params* p  = model->params;
  const States* x  = (const States*)model->states;
  States*       dx = (States*)model->stateDerivatives;

  /* lorenz attractor state equtions */
  dx->x = p->sigma * (x->y - x->x);
  dx->y = x->x * (p->rho - x->z) - x->y;
  dx->z = x->x * x->y - p->beta * x->z;
}
```

**A model without states** is a static map **y = h(t, u)**, e.g. a lookup table. Leave out the `States` struct and `.numberOfStates`, and still give a state equation (it has nothing to compute) and an integrator: the FMU then only advances the time in a step. The output equation does all the work. See `src/models/lookup_table_1d`.

#### When the Integration Fails

After every integration sub-step the FMU checks whether a state has become NaN or infinite, and after the last sub-step it checks the Real outputs and the Real local variables the same way. If one of them is not finite, the integration has failed: the FMU puts the model back to the start of the communication step and returns `fmi2Discard`. The master can then go back to a saved FMU state and try again, without having to reset the FMU (see [Discarded Steps](csfmu_fmi2Reference.md#discarded-steps)).

So a state equation that cannot be evaluated at some point (e.g. outside the range where it is defined) can return `NAN` from `<math.h>` for a derivative. The step is then discarded and the master is told why. The `fault_injection` model does exactly this on purpose, to test the protocol:

```c
if (p->fail && model->time >= p->t_fail) {
    dx->x = NAN;               /* step will be discarded */
} else {
    dx->x = -x->x / p->tau;
}
```

The output equation may return `NAN` too, for an output or a local variable, which discards the step in the same way. This also works for a model without states. Values that the master reads with `fmi2GetReal` between steps (e.g. right after setting an input) are not checked; the next step discards.

#### Output Equation

You can define your outputs and calculate them using an output equation **y=h(t,x,u)**.
The equation can have any name.
```c
void whateverAnothername(modelInstance* model);
```
and it should calculate all outputs and write them to your `Outputs` struct, and all [local variables](#local-variables) into your `Locals` struct.

Here is an example implementation that defines all three states of the lorenz attractor as outputs.

```c
void lorenzAttractorOutputs(modelInstance* model) {
  const States* x   = (const States*)model->states;
  Outputs*      out = model->outputs;

  out->x = x->x;
  out->y = x->y;
  out->z = x->z;
}
```

#### Local variables

Some values are worth looking at, but are no signal another model connects to: a heat flow, an error term, an intermediate result. Declare them in a struct `Locals` and compute them in the output equation, together with the outputs. They are published with `causality="local"`: the integration tool can read and plot them, but it can never set them, and they do not appear in the `ModelStructure`.

The `thermal_mass` model (`src/models/thermal_mass/`) publishes its heat loss this way:

```c
/* variables.h */
typedef struct {
    double Q_loss;  /* heat flow from the mass to the ambient */
} Locals;

/* equations.c */
static double heatLoss(const Params* p, const States* x)
{
    return (x->T - p->T_amb) / p->R;
}

void thermalMassSystem(modelInstance* model)
{
    ...
    dx->T = (in->P - heatLoss(p, x)) / p->C;
}

void thermalMassOutputs(modelInstance* model)
{
    ...
    out->T_out     = x->T;
    locals->Q_loss = heatLoss(p, x);
}
```

The output equation runs at the communication points and whenever a variable was set since, so local variables are always consistent with the other variables, exactly like outputs. It does not run at the stages of the integrator, so the state equation never reads `model->locals`: a term that both equations need goes into a helper function, as `heatLoss` above.

#### Initialization

Two optional functions let your model prepare a simulation. You list them in the model definition (`.calculateParameters` and `.initialize`) next to your two equations.

**Calculated parameters** are values that follow from the parameters, such as a time constant `tau = R*C`. Declare them as fields of `Params` with `CALCULATED_PARAM(field)` and compute them in a function that reads nothing but the parameters:

```c
/* variables.c */
{ CALCULATED_PARAM(tau), .unit = "s", .variability = VAR_FIXED, .description = "Time constant tau = R*C" },

/* equations.c */
void thermalMassParameters(modelInstance* model)
{
    Params* p = model->params;
    p->tau = p->R * p->C;
}
```

The FMU calls it whenever a parameter may have changed: before a variable is read after a set, before the initialization, and at the start of a step after a set. So `tau` is always consistent with `R` and `C`, also while the integration tool is still setting them, and your equations can read `p->tau` like any parameter. The integration tool can read a calculated parameter, but never set it. Give it `.variability = VAR_FIXED` if it only depends on fixed parameters, otherwise leave the default `VAR_TUNABLE`.

**The initialize function** computes the start of the simulation from the current parameters and inputs: it runs after the start values and parameters are set and the calculated parameters are computed, and before the outputs of the first communication point. It can set initial states that the model computes itself, such as a steady state, and the [internals](#internals). It returns `SIM_OK`, or explains with `modelError` why the model cannot start:

```c
simStatus thermalMassInitialize(modelInstance* model)
{
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    States*       x  = (States*)model->states;

    if (!(p->R > 0.0) || !(p->C > 0.0)) {       /* the equations divide by R and C */
        return modelError(model, "R and C must be greater than zero (R = %g K/W, C = %g J/K).", p->R, p->C);
    }

    switch (p->init_mode) {
        case THERMAL_MASS_FIXED_START:
            return SIM_OK;                       /* T keeps its start value */
        case THERMAL_MASS_STEADY_STATE:
            x->T = p->T_amb + p->R * in->P;      /* dT/dt = 0 */
            return SIM_OK;
        default:
            return modelError(model, "init_mode %d is neither %d (fixed_start) nor %d (steady_state).",
                              p->init_mode, THERMAL_MASS_FIXED_START, THERMAL_MASS_STEADY_STATE);
    }
}
```

`modelError` takes a `printf`-like message; the FMU fails the initialization and logs it.
The initialize function is also the place to refuse parameter values your equations cannot run with. `min` and `max` in the variable table include their bounds, so `.min = SOME(0.0)` still allows 0; `thermal_mass` therefore refuses R = 0 and C = 0 here, before the first step would divide by them.

A state that the initialize function may replace is declared with `.initial = VAR_APPROX`:

```c
{ STATE(T), .start = 293.15, .unit = "K", .initial = VAR_APPROX, .description = "Temperature of the mass" },
```

Its start value is then published as a guess (`initial="approx"`): the integration tool can set it only before the initialization starts, and must not expect it to survive. The `thermal_mass` model (`src/models/thermal_mass/`) uses all three parts.

**When the initialize function runs.** In initialization mode the integration tool may set parameters and inputs and read outputs in turn, e.g. to find a consistent start of several connected FMUs. So that what it reads is what the simulation starts from, the FMU initializes your model for the current values whenever a variable is read after a set, and once more at the end of initialization mode if something was set since. Your initialize function may therefore run several times, and each run must give the start for the current values only:

- Every run starts from the same guesses: before each run, the FMU sets the states with `.initial = VAR_APPROX` back to the values they had when the initialization began. That is why `thermal_mass` in `fixed_start` mode can simply leave `T` alone, even after a run in `steady_state` mode.
- The internals keep what the previous run left in them. Use them to cache expensive work, such as a file that has already been read, but not to count runs or to accumulate.
- If your initialize function fails, the read fails too, with the same error that the end of initialization mode would report.

#### Resources

A model can carry data files, such as a table or a map. Put them into the folder `src/models/<model>/resources/`: the build packs it into the FMU as `resources/` and repackages the FMU whenever a file in it changes. When the FMU runs, the integration tool tells it where the unzipped `resources/` folder is, and your model finds it as `model->resourcePath`, an absolute path that ends with `/`.

Read your files in the initialize function (see [Initialization](#initialization)), into the [`Internals`](#internals). This is the `lookup_table_1d` model, which reads the file named by its [string parameter](#string-parameters) `table_file`:

```c
simStatus lookupTable1dInitialize(modelInstance* model)
{
    const Params* p = model->params;
    Internals* table = model->internals;

    /* the table of this file was read by an earlier run (see item 6 below) */
    if (table->numberOfPoints > 0 && strcmp(table->loadedFile, p->table_file) == 0) {
        return SIM_OK;
    }

    if (!model->resourcePath) {
        return modelError(model, "The FMU has no resource folder, so the table file '%s' cannot be read.",
                          p->table_file);
    }

    /* model->resourcePath ends with '/': append the file name */
    size_t size = strlen(model->resourcePath) + strlen(p->table_file) + 1;
    char* path = malloc(size);
    ...
    snprintf(path, size, "%s%s", model->resourcePath, p->table_file);

    FILE* file = modelOpenFile(path, "rb");
    if (!file) {
        return modelError(model, "Cannot open the table file '%s'.", path);
    }
    ...
}
```

Things to know:

1. **Open files with `modelOpenFile`.** It takes the same arguments as `fopen`, but the path is UTF-8 on every platform. On Windows `fopen` would fail as soon as the path contains a character such as `ä`, e.g. in the temporary folder of a user with such a name.
2. **Report problems with `modelError`.** A missing or invalid file fails the initialization, and the message, which should name the file by its full path, reaches the log of the integration tool.
3. **`model->resourcePath` can be NULL.** This happens if the integration tool does not give the folder as a `file:` URI. Check it before you use it, as above.
4. **Before initialization mode the data is not there yet.** Outputs that need it cannot be computed before; `lookup_table_1d` returns NaN for them until then. In initialization mode they are already computed, because a read initializes the model (see [Initialization](#initialization)).
5. **Allocate in the initialization, not in a step.** Memory for paths or buffers may be allocated in the initialize function (free it there too); the data itself belongs into the `Internals`, so that it is part of the FMU state.
6. **Read a file only once.** The initialize function may run several times in initialization mode (see [Initialization](#initialization)). `lookup_table_1d` keeps the name of the file its table came from (`loadedFile`) in its internals, next to the table, and reads the file again only for another `table_file`. `fmi2Reset` zeroes the internals, so a new simulation reads the file again.

#### C++ Models

You can also implement your equations in C++ by naming the file `equations.cpp`. This lets you use classes, the C++ standard library, templates, and other C++ features inside your model. `uv run csfmu new <model> --cpp` creates a model with its equations in C++.

The only requirement is that the two equation functions must have **C linkage** (via `extern "C"`). Everything else can be pure C++.

Here is the `pendulum_on_cart` model as an example:

```cpp
// equations.cpp
#include "variables.h"
#include <cmath>

class PendulumOnCart {
public:
    PendulumOnCart(const States& states, const Inputs& inputs, const Params& params)
        : x_dot(states.x_dot),
          theta(states.theta), theta_dot(states.theta_dot),
          F(inputs.F),
          M(params.M), m(params.m), l(params.l), g(params.g) {}

    void derivatives(States& dx) const {
        double sinTh = std::sin(theta);
        double cosTh = std::cos(theta);
        double denom = M + m * sinTh * sinTh;

        dx.x         = x_dot;
        dx.x_dot     = (F + m * sinTh * (l * theta_dot * theta_dot + g * cosTh)) / denom;
        dx.theta     = theta_dot;
        dx.theta_dot = (-F * cosTh - m * l * theta_dot * theta_dot * cosTh * sinTh
                        - (M + m) * g * sinTh) / (l * denom);
    }

private:
    double x_dot, theta, theta_dot;
    double F;
    double M, m, l, g;
};

extern "C" {

void pendulumOnCartSystem(modelInstance* model) {
    const States& states = *reinterpret_cast<const States*>(model->states);
    const Inputs& inputs = *static_cast<const Inputs*>(model->inputs);
    const Params& params = *static_cast<const Params*>(model->params);
    States&       dx     = *reinterpret_cast<States*>(model->stateDerivatives);

    PendulumOnCart p(states, inputs, params);
    p.derivatives(dx);
}

void pendulumOnCartOutputs(modelInstance* model) {
    const States& states  = *reinterpret_cast<const States*>(model->states);
    Outputs&      outputs = *static_cast<Outputs*>(model->outputs);

    outputs.x     = states.x;
    outputs.theta = states.theta;
}

}  // extern "C"
```

Key points for C++ models:
- Only the wrapper functions need `extern "C"` — internal classes and helpers do not
- The `model.h` and `integrator.h` headers already have `extern "C"` guards, so they can be `#include`d from `.cpp` files directly
- C++ does not convert `void*` implicitly, so the storage blocks need explicit casts
- `variables.c` stays a C file (see below), also for C++ models

---

### `variables.c`

This file is the **single source of truth** about your model. It contains two things: the table of published variables, and the model definition.

#### The variable table

Every variable you want to be visible from outside gets one entry. The entry starts with a macro that names the struct field:

| Macro | Declares a field of | Role |
|-------|---------------------|------|
| `PARAM(field)` | `Params` | parameter |
| `CALCULATED_PARAM(field)` | `Params` | calculated parameter (see [Initialization](#initialization)) |
| `INPUT(field)` | `Inputs` | input |
| `OUTPUT(field)` | `Outputs` | output |
| `LOCAL(field)` | `Locals` | local variable |
| `STATE(field)` | `States` | continuous state |

The macro derives the **name**, the **type** and the **storage location** from the struct field itself, so these can never get out of sync with your structs. A field with an unsupported type does not compile.

After the macro you can add optional properties:

| Property | Meaning | Example |
|----------|---------|---------|
| `.start` | Start value (default 0). For `bool` use 0 or 1 | `.start = 10.0` |
| `.startString` | String parameters only: the start text (default `""`) | `.startString = "table.csv"` |
| `.unit` | Unit name (real variables) | `.unit = "s"` |
| `.description` | Human readable description | `.description = "Prandtl number"` |
| `.min`, `.max` | Bounds | `.min = SOME(0.0)` |
| `.nominal` | Nominal value (real variables) | `.nominal = 1.0` |
| `.variability` | Parameters and calculated parameters only: `VAR_TUNABLE` (default) or `VAR_FIXED` | `.variability = VAR_FIXED` |
| `.initial` | States only: `VAR_EXACT` (default) or `VAR_APPROX` if the initialize function may replace the start value (see [Initialization](#initialization)) | `.initial = VAR_APPROX` |
| `.dependsOn` | Outputs only: the inputs the output depends on directly (see [below](#direct-feedthrough)) | `.dependsOn = ""` |
| `.declaredType` | `int` variables only: the enumeration type of the variable (see [Enumerations](#enumerations)) | `.declaredType = &INIT_MODE` |

```c
#include "variables.h"
#include "integrator.h"

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    { PARAM(sigma), .start = 10.0,      .min = SOME(0.0), .description = "Prandtl number" },
    { PARAM(rho),   .start = 28.0,      .min = SOME(0.0), .description = "Rayleigh number" },
    { PARAM(beta),  .start = 8.0 / 3.0, .min = SOME(0.0), .description = "Geometric factor" },
    { OUTPUT(x) },
    { OUTPUT(y) },
    { OUTPUT(z) },
    { STATE_AS(x, "state_x"), .start = 1.0 },
    { STATE_AS(y, "state_y"), .start = 1.0 },
    { STATE_AS(z, "state_z"), .start = 1.0 },
};
```

Things to know:

1. **The order of the entries is free.** Group them however you find readable.
2. **Names must be unique.** By default a variable is published under its field name. Here `Outputs` and `States` both have a field `x`, so the states are published under a different name with `STATE_AS(field, "name")`. The same exists as `PARAM_AS`, `CALCULATED_PARAM_AS`, `INPUT_AS`, `OUTPUT_AS` and `LOCAL_AS`.
3. **Every state must be listed.** All fields of `States` have to appear in the table. The start value of a state is its initial condition.
4. **List every parameter, input, output and local variable too.** Only listed variables appear in the `modelDescription.xml`. The build does not stop you from leaving out a field of `Params`, `Inputs`, `Outputs` or `Locals`, but such a field cannot be reached from outside: an input that is never set, an output nobody can read, a parameter without a start value. Memory that your model needs but does not publish belongs into [`Internals`](#internals).

If you break rule 2 or 3, the build stops with a message that tells you what to fix.

#### The model definition

The second part of the file ties everything together in one constant, which must be named `MODEL_DEFINITION`:

```c
const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "lorenz_attractor",
    .guid        = "{8c4e8905-3735-4ba7-ab72-cf5f62a3ef77}",
    .description = "Lorenz attractor",
    .version     = "1.0.0",

    /* variables */
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),

    /* system dimensions */
    .sizeOfParams   = sizeof(Params),
    .sizeOfOutputs  = sizeof(Outputs),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    /* model equations */
    .getStateDerivatives = lorenzAttractor,
    .getOutputs          = lorenzAttractorOutputs,

    /* method for numerical integration */
    .integrator  = &EULER_FORWARD,
    .maxStepSize = 1e-4,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 30.0, .stepSize = 0.01 },
};
```

| Field | Description |
|-------|-------------|
| `name` | Name of the model. Must match the folder name (it becomes the name of the FMU binary) |
| `guid` | Globally unique identifier for this FMU (use a UUID generator). Change it when the interface of your model changes |
| `description`, `version` | Optional meta data |
| `variables`, `numberOfVariables` | The variable table |
| `units`, `numberOfUnits` | Optional table that defines your units in SI base units (see below) |
| `sizeOfParams`, `sizeOfInputs`, `sizeOfOutputs`, `sizeOfLocals`, `sizeOfInternals` | `sizeof` of the respective struct. Leave out what your model does not have |
| `numberOfStates` | Number of states, `NUMBER_OF_DOUBLES(States)` |
| `getStateDerivatives`, `getOutputs` | Your two equations |
| `integrator` | Numerical integration method: `&EULER_FORWARD` or `&RUNGE_KUTTA_4` (see `include/integrator.h`), or one of your own (see [How to add your own Integrator](#how-to-add-your-own-integrator)) |
| `maxStepSize` | Maximum internal step size for stability |
| `calculateParameters`, `initialize` | Optional: computes the calculated parameters, and prepares the simulation (see [Initialization](#initialization)). `calculateParameters` is required if the model has calculated parameters |
| `defaultExperiment` | Optional suggested simulation settings. Only the settings you give are published: the time window if `.stopTime` is greater than `.startTime`, `.tolerance` and `.stepSize` if they are greater than 0. Its `.tolerance` is also the relative tolerance the FMU uses when the integration tool defines none or an invalid one (`1e-6` if not set) |

**Solver step size matters**: For systems like the Lorenz attractor, choose `maxStepSize` small enough to maintain numerical stability. The FMU will automatically subdivide larger simulation steps.

#### Units (optional)

If your variables have units, you can define them in terms of the SI base units (`kg`, `m`, `s`, `A`, `K`, `mol`, `cd`, `rad`). Give the exponent of each base unit; leave out the ones that are zero.

```c
static const modelUnit UNITS[] = {
    { .name = "kg",  .kg = 1 },
    { .name = "N",   .kg = 1, .m = 1, .s = -2 },
    { .name = "N/m", .kg = 1, .s = -2 },
};
```

and reference the table in the model definition with `.units = UNITS, .numberOfUnits = ARRAY_LEN(UNITS)`. See `src/models/mass_spring_damper/variables.c` for a complete example.

#### Integer and Boolean variables

A variable is an integer or a boolean simply because its struct field is an `int` or a `bool`:

```c
typedef struct {
    bool enable;  /* ramp runs while true */
    int  boost;   /* multiplier of the ramp rate */
} Inputs;

static const modelVariable VARIABLES[] = {
    { INPUT(enable), .start = 1 },
    { INPUT(boost),  .start = 1, .min = SOME(0) },
    ...
};
```

See `src/models/typed_demo` for a model that uses all three types in all roles.

#### Enumerations

An `int` variable whose values stand for named choices, such as a mode, can be published as an **enumeration**. The integration tool then shows the names instead of the numbers, and only the listed values can be set. Declare the items and the type in `variables.c`, and point the variable at its type with `.declaredType`. This is the `init_mode` parameter of the `thermal_mass` model:

```c
/* variables.h: the values of the items */
#define THERMAL_MASS_FIXED_START  1
#define THERMAL_MASS_STEADY_STATE 2

typedef struct {
    ...
    int init_mode;
} Params;

/* variables.c */
static const modelEnumerationItem INIT_MODE_ITEMS[] = {
    { .name = "fixed_start",  .value = THERMAL_MASS_FIXED_START,  .description = "T starts at its start value" },
    { .name = "steady_state", .value = THERMAL_MASS_STEADY_STATE, .description = "T starts in equilibrium" },
};

static const modelEnumeration INIT_MODE = {
    .name          = "InitMode",
    .description   = "Initialization of the temperature",
    .items         = INIT_MODE_ITEMS,
    .numberOfItems = ARRAY_LEN(INIT_MODE_ITEMS),
};

static const modelVariable VARIABLES[] = {
    { PARAM(init_mode), .declaredType = &INIT_MODE, .start = THERMAL_MASS_FIXED_START, .variability = VAR_FIXED },
    ...
};
```

Things to know:

1. **The field is an `int`.** It is set and read with `fmi2SetInteger` and `fmi2GetInteger`, like an Integer variable. Use `#define` constants (or the constants of a C `enum`) for the values, but not a C `enum` type for the field: its size and signedness depend on the compiler.
2. **The items are the only valid values.** Setting any other value, also one between two items, is an error. The start value must be one of the items, and the variable has no `.min` or `.max` of its own. The build checks this, and that the names and values of the items are unique.
3. **One type, many variables.** Several variables may point at the same type; it is published once in the `TypeDefinitions` of the `modelDescription.xml`.
4. **Computed values are your responsibility.** An output, local variable or calculated parameter can be an enumeration too; your equations must write the value of an item into it.

#### String parameters

A parameter can be a text, such as the name of a file. Declare it as a `char` array of the fixed size `MODEL_STRING_SIZE` and give its start value as `.startString`. This is the `table_file` parameter of the `lookup_table_1d` model:

```c
/* variables.h */
typedef struct {
    char table_file[MODEL_STRING_SIZE];
} Params;

/* variables.c */
static const modelVariable VARIABLES[] = {
    { PARAM(table_file), .startString = "table.csv", .variability = VAR_FIXED,
      .description = "Name of the table file with the columns u and y" },
    ...
};
```

Things to know:

1. **The size is always `MODEL_STRING_SIZE`.** A string holds at most `MODEL_STRING_SIZE - 1` (255) bytes of UTF-8 text plus the terminating `'\0'`. The limit counts bytes, not characters: `ä` takes two. The build stops if the field has another size or is a `char*` pointer, because a pointer has no room for the text.
2. **The FMU copies the text in.** `fmi2SetString` copies the text into the field, so the integration tool can free its own string after the call. A text that does not fit is refused with an error and the parameter keeps its value; it is never cut.
3. **Read it as a C string.** Your equations read `p->table_file` like any other `const char*`, but never write it.
4. **Fixed or tunable.** Like every parameter, a string is tunable unless you declare it `VAR_FIXED`. A string has no `.start`, `.unit`, `.min`, `.max` or `.nominal`, and the start text must not contain control characters such as a line break. The build checks all of this.
5. **Only parameters.** `PARAM` and `PARAM_AS` accept a string field; inputs, outputs, local variables, calculated parameters and states do not.

#### Why is this a C file?

The variable table uses two features of C that C++ does not offer in the same way (designated initializers in any order and `_Generic`). The file contains no logic, so there is nothing to gain from C++ here. A model with `equations.cpp` therefore still has a `variables.c`; see `src/models/pendulum_on_cart`.

---

### What happened to `modelDescription.xml`?

Every FMU contains a `modelDescription.xml`: the **FMU's public interface**. It tells integration tools everything they need to know about your model: its variables, capabilities, and metadata.

You do not write this file. During the build, a small program (`src/fmi2ModelDescription.c`) is linked with your model, reads its `MODEL_DEFINITION` and writes the file to `build/modelDescription.xml`, from where it is packaged into the FMU. This is how each role is translated:

| Role | causality | variability | initial |
|------|-----------|-------------|---------|
| parameter | `parameter` | `tunable` or `fixed` | `exact` |
| calculated parameter | `calculatedParameter` | `tunable` or `fixed` | `calculated` |
| input | `input` | `continuous` (real) or `discrete` (integer, enumeration, boolean) | |
| output | `output` | `continuous` (real) or `discrete` (integer, enumeration, boolean) | `calculated` |
| state | `local` | `continuous` | `exact` (`approx` with `.initial = VAR_APPROX`) |
| local variable | `local` | `continuous` (real) or `discrete` (integer, boolean) | `calculated` |

A string parameter is published like any other parameter, with its start text: `<String start="table.csv"/>`.

The model name becomes the `modelIdentifier`, which is also the name of the binary inside the FMU. The `.name` of your model definition must therefore be equal to the name of your model folder; the build stops if it is not.

The **value reference** of a variable is simply its position in your variable table. The FMI implementation uses the same rule to find a variable, which is why the two can never disagree. When you use the FMU, always address variables **by name** and let your tool look up the value reference:

```python
result = simulate_fmu(
    "build/lorenz_attractor.fmu",
    start_values={"rho": 20.0, "state_x": 2.0},
    output=["x", "y", "z"],
)
```

Since the states are published, initial conditions can be set from outside like any other start value. Parameters marked `VAR_FIXED` and states can be set until the initialization is finished; inputs and tunable parameters also between steps. A state declared with `.initial = VAR_APPROX` can only be set before the initialization starts. Outputs, local variables and calculated parameters are computed by the model and can never be set. States and local variables share the causality `local`; `initial` tells them apart.

#### Direct feedthrough

An output has *direct feedthrough* if your output equation uses an input, e.g. `y = x + u`: the output then changes in the very moment the input is set. Integration tools need to know this to find algebraic loops when FMUs are connected in a feedback loop.

Only you know what your output equation does, so you declare it with `.dependsOn`, which takes the names of the inputs separated by spaces:

```c
    { OUTPUT(y), .dependsOn = "" },      /* y = x        : no input             */
    { OUTPUT(e), .dependsOn = "u" },     /* e = u - x    : only u               */
    { OUTPUT(z), .dependsOn = "u w" },   /* z = u + w    : u and w              */
    { OUTPUT(v) },                       /* not declared : assumed to be all    */
```

If you declare nothing, the `modelDescription.xml` says that the output depends on **all** inputs. That is always safe, but tools may then report algebraic loops that do not exist. Most models compute their outputs from the states only, so `.dependsOn = ""` is the common case.

The build checks that the names are inputs of your model, and the test suite (`tests/integration/test_feedthrough.py`) checks your declaration against the FMU in both directions:

- An output must **not** change when an input it does not depend on is set.
- An output **must** change right after an input it declares is set, without a simulation step in between.

FMI itself only says that a declared output *may* depend on the input. The second check is stricter, so declare only real dependencies, and choose start values at which the dependency is active (e.g. a gain that is not 0, an input that is not in saturation). An undeclared output (all inputs) is not checked in this direction.

The `pi_controller` model (`src/models/pi_controller/`) is the worked example: `e = r - y_m` and `u = Kp*e + Ki*x_I` both declare `.dependsOn = "r y_m"`, and `tests/models/pi_controller/test_feedthrough_oracle.py` checks the values right after a set.

#### Quick Checklist

Before building your FMU, verify:

- [ ] `.name` matches your model folder name
- [ ] `.guid` is a new, unique identifier
- [ ] Every field of `States` is listed in the variable table
- [ ] All `.start` values make sense for your model

---

## How to add your own Integrator

An integration method is described by an `integratorDef` (`include/model.h`): a name, a step function, the work memory it needs and its order. `EULER_FORWARD` and `RUNGE_KUTTA_4` in `src/integrator.c` are built that way, and your model can bring its own descriptor in the same form.

The step function takes one step from the current time:
```c
simStatus step(modelInstance* model, double hMax, double* hTaken);
```

The contract is (see `include/integrator.h`):
* On entry **model->time** is the time **t** at the start of the step, and **model->stateDerivatives** holds the derivatives at **t** and the current state: the simulation has evaluated them right before the call
* The function updates **model->states** in-place with the result of the step and writes the length of the step to **\*hTaken**. A fixed-step method takes the whole step: `*hTaken = hMax`
* If your method needs further derivative evaluations (like multi-stage methods), set **model->states** and **model->time** to the values of that stage and call **evaluateDerivatives(model)** (`include/evaluate.h`), never the state equation directly. If it does not return `SIM_OK`, return that status at once. Setting the stage time matters: otherwise a model that depends on time loses the order of your method
* On return **model->time** must be **t + \*hTaken**. The simulation checks this after every step: an integrator that takes another step or leaves another time (beyond rounding) makes the step fail with an error
* Return `SIM_OK`. The content of **model->stateDerivatives** after the step does not matter: the derivatives are evaluated again at the start of the next step
* For intermediate results use **model->scratch**. It holds as many vectors of length nx as your descriptor asks for (`scratchVectors`), allocated once per instance. Do not use variable-length arrays (`double k1[nx]`): MSVC does not support them

Here is the **explicit midpoint method** (2nd-order Runge-Kutta). Put it into your model's `variables.c`, above the `MODEL_DEFINITION` (the build compiles a model's `variables.c` and its equations file):

```c
#include <string.h>
#include "evaluate.h"

static simStatus midpointStep(modelInstance* model, double hMax, double* hTaken)
{
    double t0 = model->time;
    size_t nx = model->def->numberOfStates;
    *hTaken = hMax;  /* a fixed-step method takes the whole step */
    if (nx == 0) {
        model->time = t0 + hMax;
        return SIM_OK;
    }

    double* k1 = model->scratch;  /* 2 vectors: see scratchVectors below */
    double* stateBackup = k1 + nx;

    /* k1 = f(t, x), evaluated before the call */
    memcpy(k1, model->stateDerivatives, nx * sizeof(double));
    memcpy(stateBackup, model->states, nx * sizeof(double));

    /* evaluate at the midpoint: f(t + h/2, x + h/2 * k1) */
    model->time = t0 + 0.5 * hMax;
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + 0.5 * hMax * k1[i];
    }
    simStatus status = evaluateDerivatives(model);
    if (status != SIM_OK) {
        return status;
    }

    /* x(t + h) = x(t) + h * f(midpoint) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + hMax * model->stateDerivatives[i];
    }
    model->time = t0 + hMax;
    return SIM_OK;
}

static const integratorDef MIDPOINT = {
    .name           = "explicit midpoint",
    .step           = midpointStep,
    .scratchVectors = 2,
    .order          = 2,
};
```

Then select it in the same file's model definition:

```c
    .integrator  = &MIDPOINT,
```

`src/integrator.c` holds the built-in methods as reference implementations. A method that every model should be able to use belongs there, with an `extern` declaration in `include/integrator.h`.

To see how your integrator changes a trajectory, record the model with `csfmu simulate` before and after you switch it, and compare the two files (see [simulation](csfmu_simulation.md#comparing-two-builds)).
