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
  | `test/` | Unit and integration tests |

**Note on examples:** The C examples (`example_model.c`, `example_fmu.c`) are self-contained references for the `first_order` model and only build when `-DMODEL=first_order` is selected. Each model also has a Python example script (`examples/example_<model_name>.py`) that demonstrates simulation with fmpy.

## Core principle of csfmu

This template is offering an expandable library of models with minimum restrictive implementation specifications alongside a model independent fmi implementation.

The two sides are kept apart by one idea: **a model describes its variables once, in a plain C table.**

* The **model** knows nothing about FMI. It declares its parameters, inputs, outputs and states as ordinary C structs and lists them in a table (name, start value, unit, ...).
* The **FMI implementation** knows nothing about the model. It finds every variable through that table.
* The **`modelDescription.xml`** is generated from the same table during the build. You never write it by hand, and you never deal with value references.

As a consequence there are no ordering rules: variables can be declared in any order and can be of type `double`, `int` or `bool`.

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

That is all. You do not have to touch any other file of the project: the model is picked up by `-DMODEL=lorenz_attractor`.

**Tip:** `uv run csfmu new lorenz_attractor` creates the folder and all three files for you, filled with a simple integrator that builds and passes the tests right away. See [the project manager](csfmu_cli.md) for details.

The build system auto-detects whether your model uses C or C++ based on the file extension. If `equations.cpp` exists it will be compiled with the C++ compiler; otherwise `equations.c` is used.

Let's take a look at these files one by one in detail.

---

### `variables.h`

This header declares the storage of your model as four plain structs. The struct names are fixed, the fields are up to you:

| Struct | Content | Allowed field types |
|--------|---------|---------------------|
| `Params` | Model parameters | `double`, `int`, `bool` |
| `Inputs` | Input signals | `double`, `int`, `bool` |
| `Outputs` | Output signals | `double`, `int`, `bool` |
| `States` | Continuous states | `double` only |

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

#### When the Integration Fails

After every integration sub-step the FMU checks whether a state has become NaN or infinite. If one has, the integration has failed: the FMU puts the model back to the start of the communication step and returns `fmi2Discard`. The master can then go back to a saved FMU state and try again, without having to reset the FMU (see [Discarded Steps](csfmu_fmi2Reference.md#discarded-steps)).

So a state equation that cannot be evaluated at some point (e.g. outside the range where it is defined) can return `NAN` from `<math.h>` for a derivative. The step is then discarded and the master is told why. The `fault_injection` model does exactly this on purpose, to test the protocol:

```c
if (p->fail && model->time >= p->t_fail) {
    dx->x = NAN;               /* step will be discarded */
} else {
    dx->x = -x->x / p->tau;
}
```

#### Output Equation

You can define your outputs and calculate them using an output equation **y=h(t,x,u)**.
The equation can have any name.
```c
void whateverAnothername(modelInstance* model);
```
and it should calculate all outputs and write them to your `Outputs` struct.

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

#### C++ Models

You can also implement your equations in C++ by naming the file `equations.cpp`. This lets you use classes, the C++ standard library, templates, and other C++ features inside your model.

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
| `INPUT(field)` | `Inputs` | input |
| `OUTPUT(field)` | `Outputs` | output |
| `STATE(field)` | `States` | continuous state |

The macro derives the **name**, the **type** and the **storage location** from the struct field itself, so these can never get out of sync with your structs. A field with an unsupported type does not compile.

After the macro you can add optional properties:

| Property | Meaning | Example |
|----------|---------|---------|
| `.start` | Start value (default 0). For `bool` use 0 or 1 | `.start = 10.0` |
| `.unit` | Unit name (real variables) | `.unit = "s"` |
| `.description` | Human readable description | `.description = "Prandtl number"` |
| `.min`, `.max` | Bounds | `.min = SOME(0.0)` |
| `.nominal` | Nominal value (real variables) | `.nominal = 1.0` |
| `.variability` | Parameters only: `VAR_TUNABLE` (default) or `VAR_FIXED` | `.variability = VAR_FIXED` |
| `.dependsOn` | Outputs only: the inputs the output depends on directly (see [below](#direct-feedthrough)) | `.dependsOn = ""` |

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
2. **Names must be unique.** By default a variable is published under its field name. Here `Outputs` and `States` both have a field `x`, so the states are published under a different name with `STATE_AS(field, "name")`. The same exists as `PARAM_AS`, `INPUT_AS` and `OUTPUT_AS`.
3. **Every state must be listed.** All fields of `States` have to appear in the table. The start value of a state is its initial condition.
4. **List every parameter, input and output too.** Only listed variables appear in the `modelDescription.xml`. The build does not stop you from leaving out a field of `Params`, `Inputs` or `Outputs`, but such a field cannot be reached from outside and stays at zero: an input that is never set, an output nobody can read, a parameter without a start value.

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
    .integrate   = eulerForward,
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
| `sizeOfParams`, `sizeOfInputs`, `sizeOfOutputs` | `sizeof` of the respective struct. Leave out what your model does not have |
| `numberOfStates` | Number of states, `NUMBER_OF_DOUBLES(States)` |
| `getStateDerivatives`, `getOutputs` | Your two equations |
| `integrate` | Numerical integration method to use, see `include/integrator.h` |
| `maxStepSize` | Maximum internal step size for stability |
| `defaultExperiment` | Optional suggested simulation settings. Its `.tolerance` is also the relative tolerance the FMU uses when the integration tool defines none (`1e-6` if not set) |

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

#### Why is this a C file?

The variable table uses two features of C that C++ does not offer in the same way (designated initializers in any order and `_Generic`). The file contains no logic, so there is nothing to gain from C++ here. A model with `equations.cpp` therefore still has a `variables.c`; see `src/models/pendulum_on_cart`.

---

### What happened to `modelDescription.xml`?

Every FMU contains a `modelDescription.xml`: the **FMU's public interface**. It tells integration tools everything they need to know about your model: its variables, capabilities, and metadata.

You do not write this file. During the build, a small program (`src/fmi2ModelDescription.c`) is linked with your model, reads its `MODEL_DEFINITION` and writes the file to `build/modelDescription.xml`, from where it is packaged into the FMU. This is how each role is translated:

| Role | causality | variability | initial |
|------|-----------|-------------|---------|
| parameter | `parameter` | `tunable` or `fixed` | `exact` |
| input | `input` | `continuous` (real) or `discrete` (integer, boolean) | |
| output | `output` | `continuous` (real) or `discrete` (integer, boolean) | `calculated` |
| state | `local` | `continuous` | `exact` |

The model name becomes the `modelIdentifier`, which is also the name of the binary inside the FMU. The `.name` of your model definition must therefore be equal to the name of your model folder; the build stops if it is not.

The **value reference** of a variable is simply its position in your variable table. The FMI implementation uses the same rule to find a variable, which is why the two can never disagree. When you use the FMU, always address variables **by name** and let your tool look up the value reference:

```python
result = simulate_fmu(
    "build/lorenz_attractor.fmu",
    start_values={"rho": 20.0, "state_x": 2.0},
    output=["x", "y", "z"],
)
```

Since the states are published, initial conditions can be set from outside like any other start value. Parameters marked `VAR_FIXED` and states can be set until the initialization is finished; inputs and tunable parameters also between steps.

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

The build checks that the names are inputs of your model, and the test suite (`test/integration/test_feedthrough.py`) checks your declaration against the FMU in both directions:

- An output must **not** change when an input it does not depend on is set.
- An output **must** change right after an input it declares is set, without a simulation step in between.

FMI itself only says that a declared output *may* depend on the input. The second check is stricter, so declare only real dependencies, and choose start values at which the dependency is active (e.g. a gain that is not 0, an input that is not in saturation). An undeclared output (all inputs) is not checked in this direction.

The `pi_controller` model (`src/models/pi_controller/`) is the worked example: `e = r - y_m` and `u = Kp*e + Ki*x_I` both declare `.dependsOn = "r y_m"`, and `test/models/pi_controller/test_feedthrough_oracle.py` checks the values right after a set.

#### Quick Checklist

Before building your FMU, verify:

- [ ] `.name` matches your model folder name
- [ ] `.guid` is a new, unique identifier
- [ ] Every field of `States` is listed in the variable table
- [ ] All `.start` values make sense for your model

---

## How to add your own Integrator

All integrators share the same signature defined in `include/model.h`:
```c
void yourIntegrator(modelInstance* model, double stepSize);
```

The contract is:
* On entry **model->time** is the time **t** at the start of the step, and **model->stateDerivatives** contains the derivatives at **t** and the current state (pre-computed before the call)
* The function must update **model->states** in-place with the result of the integration step
* If your method needs intermediate derivative evaluations (like multi-stage methods), call **model->getStateDerivatives(model)** internally after temporarily modifying **model->states** and setting **model->time** to the time of that stage. Otherwise a model that depends on time loses the order of your method
* On return **model->time** must be **t + stepSize**
* For intermediate results use **model->scratch**. It holds `MODEL_SCRATCH_VECTORS` (5) vectors of length nx and is allocated once per instance. Do not use variable-length arrays (`double k1[nx]`): MSVC does not support them

First, add your implementation to `src/integrator.c` and its declaration to `include/integrator.h`.

Here is a minimal example of the **explicit midpoint method** (2nd-order Runge-Kutta):

```c
void midpoint(modelInstance* model, double stepSize)
{
    double t0 = model->time;
    size_t nx = model->def->numberOfStates;
    if (nx == 0) {
        model->time = t0 + stepSize;
        return;
    }

    double* k1 = model->scratch;
    double* stateBackup = k1 + nx;

    /* k1 = f(t, x) — reuse pre-computed derivatives */
    memcpy(k1, model->stateDerivatives, nx * sizeof(double));
    memcpy(stateBackup, model->states, nx * sizeof(double));

    /* evaluate at midpoint: f(t + h/2, x + h/2 * k1) */
    model->time = t0 + 0.5 * stepSize;
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + 0.5 * stepSize * k1[i];
    }
    model->getStateDerivatives(model);

    /* x(t+h) = x(t) + h * f(midpoint) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + stepSize * model->stateDerivatives[i];
    }
    model->time = t0 + stepSize;
}
```

Then, select it in the model definition in your model's `variables.c`:

```c
    .integrate = midpoint,
```

See **eulerForward** and **rungeKutta4** in `src/integrator.c` for reference implementations.
