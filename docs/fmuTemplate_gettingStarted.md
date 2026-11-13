# Getting started with fmuTemplate

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

## Core principal of fmuTemplate

This template is offering an expandable library of models with minimum restrictive implementation specifications alongside a model independent fmi implementation.

The current model implementation restrictions are:
* all model parameter, inputs, ouputs, states and state derivatives have double precision and are thus of type **fmi2Real**.
* the order of **ModelVariables** in the `modelDescription.xml` is fixed to allow a model independent impementation of **fmi2GetReal** and **fmi2SetReal**.

## How to Build the FMU

Run the following to build a specific **model_name**, which needs to match a folder name under `src/models`.
For example if you want to build the **first_order** model and package the fmu to a folder called `fmus` inside the root, run

```bash
rm -rf build
mkdir -p build && cd build
cmake -DMODEL=first_order -DFMU_OUTPUT_DIR=../fmus ..
cmake --build .
```

## How to add your own Model

Start by picking an apropreate **name** for you model.
Let's go with the famous **lorenz_attractor** for the sake of this tutorial.

First, create a new folder called `src/models/lorenz_attractor`, for your model.

Second, add the following files and fill them with life:

| File | Content |
|------|-------------|
|**equations.c** or **equations.cpp** | The state and output equations, that describe your model (C or C++) |
| **model_config.h** | Mappings, that connect your model with the FMI, as well as the configuration of the solver, model dimensions, initial states, etc. |
| **modelDescription.xml** | FMI specific meta data about your FMU

The build system auto-detects whether your model uses C or C++ based on the file extension. If `equations.cpp` exists it will be compiled with the C++ compiler; otherwise `equations.c` is used.

Let's take a look at these files one by one in detail.

---

### `equations.c` (or `equations.cpp`)

In this file you should implement two functions, the models **state equations** and **output equation**.

#### State Equation

This template assumes that your model can be represented as a potentially nonlinear differential equation, such that the derivatives of the states can be computed by a function **dx=f(t,x,u)**, where **dx** are the state derivatives with respect to time **t**, **x** are the states and **u** are the inputs to the model.
**f** is the state equation you have to implement here.
It can have any name.
The signature of the function is
```c
void whateverTheName(modelInstance* model);
```
and it should calculate all state derivatives and write them to
```c
model->stateDerivatives[k]
```
for all **k** in **model->info.numberOfStates**.

Here is an exampe implemenation of the **lorenz_attractor**

```c
void lorenzAttractor(modelInstance* model) {
  /* get parameter for readability */
  double sigma = model->parameter[0];
  double rho   = model->parameter[1];
  double beta  = model->parameter[2];
  /* get states readability */
  double x1 = model->states[0];
  double x2 = model->states[1];
  double x3 = model->states[2];
  /* lorenz attractor state equtions */
  double dx1 = sigma*(x2-x1);
  double dx2 = x1*(rho-x3)-x2;
  double dx3 = x1*x2-beta*x3;
  /* write back to model */
  model->stateDerivatives[0] = dx1;
  model->stateDerivatives[1] = dx2;
  model->stateDerivatives[2] = dx3;
}
```

#### Output Equation

You can define your outputs and calculate them using an output equation **y=h(t,x,u)**.
The equation can have any name.
```c
void whateverAnothername(modelInstance* model);
```
and it should calculate all outputs and write them to
```c
model->outputs[k]
```
for all **k** in **model->info->numberOfOutputs**.

Here is an example implementation that defines all three states of the lorenz attractor as outputs.

```c
void lorenzAttractorOutputs(modelInstance* model) {
  for (int k=0; k<model.info->numberOfStates; k++) {
    model->outputs[k] = model->states[k];
  }
}
```

#### C++ Models

You can also implement your equations in C++ by naming the file `equations.cpp`. This lets you use classes, the C++ standard library, templates, and other C++ features inside your model.

The only requirement is that the two functions assigned to the model's function pointers must have **C linkage** (via `extern "C"`). Everything else can be pure C++.

Here is the `pendulum_on_cart` model as an example:

```cpp
// equations.cpp
#include "model.h"
#include <cmath>

class PendulumOnCart {
public:
    PendulumOnCart(double* states, double* inputs, double* params)
        : x_dot(states[1]),
          theta(states[2]), theta_dot(states[3]),
          F(inputs[0]),
          M(params[0]), m(params[1]), l(params[2]), g(params[3]) {}

    void derivatives(double* dx) const {
        double sinTh = std::sin(theta);
        double cosTh = std::cos(theta);
        double denom = M + m * sinTh * sinTh;

        dx[0] = x_dot;
        dx[1] = (F + m * sinTh * (l * theta_dot * theta_dot + g * cosTh)) / denom;
        dx[2] = theta_dot;
        dx[3] = (-F * cosTh - m * l * theta_dot * theta_dot * cosTh * sinTh
                 - (M + m) * g * sinTh) / (l * denom);
    }

private:
    double x_dot, theta, theta_dot;
    double F;
    double M, m, l, g;
};

extern "C" {

void pendulumOnCartSystem(modelInstance* model) {
    PendulumOnCart p(model->states, model->inputs, model->parameter);
    p.derivatives(model->stateDerivatives);
}

void pendulumOnCartOutputs(modelInstance* model) {
    model->outputs[0] = model->states[0];
    model->outputs[1] = model->states[2];
}

}  // extern "C"
```

Key points for C++ models:
- Only the wrapper functions need `extern "C"` — internal classes and helpers do not
- The `model.h` and `integrator.h` headers already have `extern "C"` guards, so they can be `#include`d from `.cpp` files directly
- The `model_config.h` forward declarations are included inside the `extern "C"` block in `model.h`, so no additional guards are needed there

---

### `model_config.h`

This file is the compile-time glue between your model equations and the FMU infrastructure. It uses C preprocessor macros and definitions to configure your specific model without requiring any changes to the core FMU code via compile-time polymorphism.

#### How It Works in the Architecture

When you build with -DMODEL=lorenz_attractor, CMake:
1. Converts the model name to uppercase: LORENZ_ATTRACTOR
2. Creates a preprocessor definition: MODEL_LORENZ_ATTRACTOR
3. This triggers conditional compilation in `include/model.h`:
   ```c
   #if defined(MODEL_LORENZ_ATTRACTOR)
       #include "lorenz_attractor/model_config.h"
   #endif
   ```

#### A Note on the Include Guard

You'll notice that all `model_config.h` files share the same `#ifndef MODEL_CONFIG_H` include guard. This is intentional — it's part of the compile-time polymorphism design. Since CMake selects exactly one model per build (`-DMODEL=<name>`), only one `model_config.h` is ever included. They all represent the same configuration slot, so they share the same guard. You do **not** need unique guards per model.

#### Required Definitions

Your `model_config.h` must provide the following definitions. Here's the complete specification using the **Lorenz attractor** as an example:

| Name | Description | Example (Lorenz Attractor) |
|------|-------------|----------------------------|
| State equation function | Forward declaration of the function that computes state derivatives | void lorenzAttractor(modelInstance* model); |
| Output equation function | Forward declaration of the function that computes outputs | void lorenzAttractorOutputs(modelInstance* model); |
| GUID | Globally unique identifier for this FMU (use a UUID generator) | "{8c4e8905-3735-4ba7-ab72-cf5f62a3ef77}" |
| MODELNAME | Human-readable name for the FMU | "lorenzAttractor" |
| NUMBER_OF_PARAMS | Number of model parameters | 3 |
| NUMBER_OF_INPUTS | Number of input signals | 0 |
| NUMBER_OF_OUTPUTS | Number of output signals | 3 |
| NUMBER_OF_STATES | Number of state variables | 3 |
| INITIAL_STATE | Initial values for all state variables | {1.0, 1.0, 1.0} |
| INITIAL_PARAMS | Default parameter values (σ, ρ, β) | {10.0, 28.0, 8.0/3.0} |
| INTEGRATOR_METHOD | Numerical integration method to use | eulerForward |
| MAX_SOLVER_STEPSIZE | Maximum internal step size for stability | 1e-4 |
| MODEL_STATE_EQUATION | Maps to your state equation function | lorenzAttractor |
| MODEL_OUTPUT_EQUATION | Maps to your output equation function | lorenzAttractorOutputs |
| LOG_INFO | Category name for info messages | "INFO" |
| LOG_ERROR | Category name for error messages | "ERROR" |
| LOG_WARNING | Category name for warning messages | "WARNING" |

#### Complete Example

Here's what the complete `src/models/lorenz_attractor/model_config.h` would look like:

```c
#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

/* Forward declarations of model equations */
void lorenzAttractor(modelInstance* model);
void lorenzAttractorOutputs(modelInstance* model);

/* Model identity */
#define GUID "{8c4e8905-3735-4ba7-ab72-cf5f62a3ef77}"
#define MODELNAME "lorenzAttractor"

/* System dimensions */
#define NUMBER_OF_PARAMS  3  /* sigma, rho, beta */
#define NUMBER_OF_INPUTS  0  /* No external inputs */
#define NUMBER_OF_OUTPUTS 3  /* x, y, z */
#define NUMBER_OF_STATES  3  /* x, y, z */

/* Initial conditions: slightly perturbed from origin */
static const double INITIAL_STATE[NUMBER_OF_STATES] = {1.0, 1.0, 1.0};

/* Classic Lorenz parameters (chaotic regime) */
static const double INITIAL_PARAMS[NUMBER_OF_PARAMS] = {
    10.0,      /* sigma (Prandtl number) */
    28.0,      /* rho (Rayleigh number) */
    8.0/3.0    /* beta (geometric factor) */
};

/* Solver configuration */
#define INTEGRATOR_METHOD eulerForward
#define MAX_SOLVER_STEPSIZE 1e-4

/* Function mapping */
#define MODEL_STATE_EQUATION lorenzAttractor
#define MODEL_OUTPUT_EQUATION lorenzAttractorOutputs

/* Logging categories */
#define LOG_INFO      "INFO"
#define LOG_ERROR     "ERROR"
#define LOG_WARNING   "WARNING"

#endif
```

#### Key Design Points

1. **Array sizes must match dimensions**: The size of `INITIAL_STATE` must equal `NUMBER_OF_STATES`, and `INITIAL_PARAMS` must equal `NUMBER_OF_PARAMS`.

2. **Function names are mapped via macros**: The core FMU code calls `MODEL_STATE_EQUATION(model)`, which expands to `lorenzAttractor(model)` at compile time.

3. **No runtime overhead**: All configuration is resolved during compilation - no function pointers or lookup tables needed.

4. **Solver step size matters**: For stiff systems like the Lorenz attractor, choose `MAX_SOLVER_STEPSIZE` small enough to maintain numerical stability. The FMU will automatically subdivide larger simulation steps.

---

### `modelDescription.xml`

This XML file is the **FMU's public interface** - it tells integration tools everything they need to know about your model: its variables, capabilities, and metadata. It must be placed in the root of the FMU archive and follow the FMI 2.0 standard schema.

#### Critical Ordering Requirement

**IMPORTANT**: Due to fmuTemplate's generic variable access implementation, the `ModelVariables` section **must follow a strict ordering**:

1. **All parameters** (in order: VR 0, 1, 2, ..., NUMBER_OF_PARAMS-1)
2. **All inputs** (in order: VR NUMBER_OF_PARAMS, ..., NUMBER_OF_PARAMS + NUMBER_OF_INPUTS-1)
3. **All outputs** (in order: VR NUMBER_OF_PARAMS + NUMBER_OF_INPUTS, ...)

This ordering allows the **fmi2GetReal** and **fmi2SetReal** implementations to use simple arithmetic to map value references to array indices:

```c
/* In fmi2GetReal: */
if (VR < NUMBER_OF_PARAMS) {
    value[k] = model->parameter[VR];
}
else if (VR < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS) {
    value[k] = model->inputs[VR - NUMBER_OF_PARAMS];
}
else if (VR < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS + NUMBER_OF_OUTPUTS) {
    value[k] = model->outputs[VR - NUMBER_OF_PARAMS - NUMBER_OF_INPUTS];
}
```

**Breaking this ordering will cause incorrect variable access!**

#### File Structure Overview

The `modelDescription.xml` consists of several major sections:

| Section | Purpose | Required |
|---------|---------|----------|
| `<fmiModelDescription>` | Root element with FMU metadata (name, GUID, version) | Yes |
| `<CoSimulation>` | Declares co-simulation capabilities and limitations | Yes |
| `<UnitDefinitions>` | Defines physical units used by variables | Optional |
| `<LogCategories>` | Declares available logging categories | Optional |
| `<DefaultExperiment>` | Suggests default simulation settings | Optional |
| `<ModelVariables>` | **The heart of the file** - declares all exposed variables | Yes |
| `<ModelStructure>` | Declares variable dependencies for advanced tools | Optional |

#### Detailed Breakdown (Lorenz Attractor Example)

##### 1. Root Element & Metadata

```xml
<fmiModelDescription
    fmiVersion="2.0"
    modelName="lorenz_attractor"
    guid="{8c4e8905-3735-4ba7-ab72-cf5f62a3ef77}"
    description="Lorenz attractor chaotic system"
    author="Your Name"
    version="1.0.0"
    generationTool="fmuTemplate"
    generationDateAndTime="2025-02-04T12:00:00Z"
    numberOfEventIndicators="0">
```

- `guid`: Must match the `GUID` in `model_config.h`
- `modelName`: Should match your model folder name
- `numberOfEventIndicators`: Set to `"0"` for continuous systems

##### 2. Co-Simulation Capabilities

```xml
<CoSimulation
    modelIdentifier="your_model_name"
    needsExecutionTool="false"
    canHandleVariableCommunicationStepSize="true"
    canInterpolateInputs="false"
    maxOutputDerivativeOrder="0"
    canGetAndSetFMUstate="false"
    canSerializeFMUstate="false"
    providesDirectionalDerivative="false">
</CoSimulation>
```

These flags declare what the FMU can do. For fmuTemplate:
- `canHandleVariableCommunicationStepSize="true"`: Supports different step sizes
- Most advanced features set to `"false"` (state serialization, derivatives, etc.)

##### 3. Unit Definitions (Optional)

```xml
<UnitDefinitions>
    <Unit name="s">
        <BaseUnit s="1"/>
    </Unit>
    <Unit name="1">
        <BaseUnit/>  <!-- Dimensionless -->
    </Unit>
</UnitDefinitions>
```

Define physical units for your variables. Common base units: `s` (seconds), `m` (meters), `kg`, etc.

##### 4. Default Experiment (Optional)

```xml
<DefaultExperiment
    startTime="0.0"
    stopTime="100.0"
    tolerance="1e-6"
    stepSize="0.01"/>
```

Suggests default simulation settings. Integration tools may override these.

##### 5. Model Variables (CRITICAL)

This is where you declare all parameters, inputs, and outputs **in the required order**:

**Parameters (σ, ρ, β) - VR 0, 1, 2:**

```xml
<ScalarVariable
    name="sigma"
    valueReference="0"
    description="Prandtl number"
    causality="parameter"
    variability="tunable"
    initial="exact">
    <Real start="10.0" unit="1" min="0.0" nominal="10.0"/>
</ScalarVariable>

<ScalarVariable
    name="rho"
    valueReference="1"
    description="Rayleigh number"
    causality="parameter"
    variability="tunable"
    initial="exact">
    <Real start="28.0" unit="1" min="0.0" nominal="28.0"/>
</ScalarVariable>

<ScalarVariable
    name="beta"
    valueReference="2"
    description="Geometric factor"
    causality="parameter"
    variability="tunable"
    initial="exact">
    <Real start="2.667" unit="1" min="0.0" nominal="2.667"/>
</ScalarVariable>
```

**Inputs - VR starts at NUMBER_OF_PARAMS:**

The Lorenz attractor has no inputs, so this section would be empty. If you had inputs, they'd start at VR 3.

**Outputs (x, y, z) - VR 3, 4, 5:**

```xml
<ScalarVariable
    name="x"
    valueReference="3"
    description="First state of Lorenz system"
    causality="output"
    variability="continuous"
    initial="calculated">
    <Real unit="1"/>
</ScalarVariable>

<ScalarVariable
    name="y"
    valueReference="4"
    description="Second state of Lorenz system"
    causality="output"
    variability="continuous"
    initial="calculated">
    <Real unit="1"/>
</ScalarVariable>

<ScalarVariable
    name="z"
    valueReference="5"
    description="Third state of Lorenz system"
    causality="output"
    variability="continuous"
    initial="calculated">
    <Real unit="1"/>
</ScalarVariable>
```

**Key attributes:**
- `causality`: `"parameter"`, `"input"`, or `"output"`
- `variability`:
  - `"tunable"` for parameters that can change during initialization
  - `"continuous"` for time-varying signals
- `initial`:
  - `"exact"` for parameters (value set by `start` attribute)
  - `"calculated"` for outputs (computed by model)
- `start`: Initial/default value

##### 6. Model Structure (Optional)

```xml
<ModelStructure>
    <Outputs>
        <Unknown index="4" dependencies=""/>  <!-- x -->
        <Unknown index="5" dependencies=""/>  <!-- y -->
        <Unknown index="6" dependencies=""/>  <!-- z -->
    </Outputs>
</ModelStructure>
```

**NOTE**: Indices are **1-based** and refer to position in `ModelVariables` (parameter σ is index 1, not 0).

The `dependencies` attribute can list which inputs each output depends on. Empty means "depends on everything" or "unknown dependencies".

#### Quick Checklist

Before building your FMU, verify:

- [ ] `guid` matches `model_config.h`
- [ ] `modelName` matches your model folder name
- [ ] Variables are ordered: **all parameters, then all inputs, then all outputs**
- [ ] Value references are sequential starting from 0
- [ ] Number of variables matches dimensions in `model_config.h`
- [ ] All `start` values make sense for your model

---

## How to add your own Integrator

All integrators share the same signature defined in `include/model.h`:
```c
void yourIntegrator(modelInstance* model, double stepSize);
```

The contract is:
* **model->stateDerivatives** contains the derivatives at the current state (pre-computed before the call)
* The function must update **model->states** in-place with the result of the integration step
* If your method needs intermediate derivative evaluations (like multi-stage methods), call **model->getStateDerivatives(model)** internally after temporarily modifying **model->states**

First, add your implementation to `src/integrator.c` and its declaration to `include/integrator.h`.

Here is a minimal example of the **explicit midpoint method** (2nd-order Runge-Kutta):

```c
void midpoint(modelInstance* model, double stepSize)
{
    size_t nx = model->info->numberOfStates;
    double k1[nx], stateBackup[nx];

    /* k1 = f(x) — reuse pre-computed derivatives */
    memcpy(k1, model->stateDerivatives, nx * sizeof(double));
    memcpy(stateBackup, model->states, nx * sizeof(double));

    /* evaluate at midpoint: f(x + h/2 * k1) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + 0.5 * stepSize * k1[i];
    }
    model->getStateDerivatives(model);

    /* x(t+h) = x(t) + h * f(midpoint) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + stepSize * model->stateDerivatives[i];
    }
}
```

Then, set **INTEGRATOR_METHOD** in your model's `model_config.h` to use it:

```c
#define INTEGRATOR_METHOD midpoint
```

See **eulerForward** and **rungeKutta4** in `src/integrator.c` for reference implementations.
