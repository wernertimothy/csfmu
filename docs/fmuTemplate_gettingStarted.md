# Getting started with fmuTemplate

**Welcome.**

This overview will give you all information necessary to start implementing your own model equations, compile it to an FMU and test it, using this template.

The Project is structured as follows.

## The Folders

  | Directory | Contents |
  |-----------|----------|
  | `docs/` | References and documentation |
  | `examples/` | Example implementations |
  | `external/` | Third-party dependencies (FMI headers, Unity) |
  | `include/` | Project header files |
  | `scripts/` | Build and packaging scripts |
  | `src/` | Core FMU and model-interface implementation |
  | `src/models/` | **Model implementations (this is where you will add your model)** |
  | `test/` | Unit and integration tests |

## Core principal of fmuTemplate

This template is offering an expandable library of models with minimum restrictive implementation specifications alongside a model independent fmi implementation.

The current model implementation restrictions are:
* all model parameter, inputs, ouputs, states and state derivatives have double precision and are thus of type **fmi2Real**.
* the order of **ModelVariables** in the `modelDescription.xml` is fixed to allow a model independent impementation of **fmi2GetReal** and **fmi2SetReal**.

## How to Build the FMU

Run the following to build a specific **model_name**, which needs to match a folder name under `src/models`.
For example if you want to build the **first_order** model, run

```bash
rm -rf build
mkdir -p build && cd build
cmake -DMODEL=first_order ..
cmake --build .
```

## How to add your own Model

Start by picking an apropreate **name** for you model.
Let's go with the famous **lorenz_attractor** for the sake of this tutorial.

First, create a new folder called `src/models/lorenz_attractor`, for your model.

Second, add the following files and fill them with life:

| File | Content |
|------|-------------|
|**equations.c** | The state and output equations, that describe your model |
| **model_config.h** | Mappings, that connect your model with the FMI, as well as the configuration of the solver, model dimensions, initial states, etc. |
| **modelDescription.xml** | FMI specific meta data about your FMU

Let's take a look at these files one by one in detail.

---

### `equations.c`

In this file you should implement two functions, the models **state equations** and **output equation**.

**State Equation**.
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
  model->stateDerivatives[1] = dx2:
  model->stateDerivatives[2] = dx3;
}
```

**Output Equation**.
You can define your outputs and calculate them using an output equation **y=h(t,x,u)**.
The equation can have any name.
```c
void whateverAnothername(modelInstance* model);
```
and it should calculate all outputs and write them to
```c
model->outputs[k]
```
for all **k** in **model->info.numberOfOutputs**.

Here is an example implementation that defines all three states of the lorenz attractor as outputs.

```c
void lorenzAttractorOutputs(modelInstance* model) {
  for (int k=0; k<model.info->numberOfStates; k++) {
    model->outputs[k] = model->states[k];
  }
}
```
---

### `model_config.h`

This file is the **compile-time glue** between your model equations and the FMU infrastructure. It uses C preprocessor macros and definitions to configure your specific model without requiring any changes to the core FMU code via **compile-time polymorphism**.

#### How It Works in the Architecture

When you build with `-DMODEL=lorenz_attractor`, CMake:
1. Converts the model name to uppercase: `LORENZ_ATTRACTOR`
2. Creates a preprocessor definition: `MODEL_LORENZ_ATTRACTOR`
3. This triggers conditional compilation in `include/model.h`:
   ```c
   #if defined(MODEL_LORENZ_ATTRACTOR)
       #include "lorenz_attractor/model_config.h"
   #endif
   ```

#### Required Definitions

Your `model_config.h` must provide the following definitions. Here's the complete specification using the **Lorenz attractor** as an example:

| Category | Name | Description | Example (Lorenz Attractor) |
|----------|------|-------------|----------------------------|
| **Function Declarations** | | | |
| | State equation function | Forward declaration of the function that computes state derivatives | `void lorenzAttractor(modelInstance* model);` |
| | Output equation function | Forward declaration of the function that computes outputs | `void lorenzAttractorOutputs(modelInstance* model);` |
| **Model Identity** | | | |
| | `GUID` | Globally unique identifier for this FMU (use a UUID generator) | `#"{8c4e8905-3735-4ba7-ab72-cf5f62a3ef77}"` |
| | `MODELNAME` | Human-readable name for the FMU | `"lorenzAttractor"` |
| **System Dimensions** | | | |
| | `NUMBER_OF_PARAMS` | Number of model parameters | `3` |
| | `NUMBER_OF_INPUTS` | Number of input signals | `0` |
| | `NUMBER_OF_OUTPUTS` | Number of output signals | `3` |
| | `NUMBER_OF_STATES` | Number of state variables | `3` |
| **Initial Values** | | | |
| | `INITIAL_STATE` | Initial values for all state variables | `{1.0, 1.0, 1.0};` |
| | `INITIAL_PARAMS` | Default parameter values (σ, ρ, β) | `{10.0, 28.0, 8.0/3.0};` |
| **Solver Configuration** | | | |
| | `INTEGRATOR_METHOD` | Numerical integration method to use | `eulerForward` |
| | `MAX_SOLVER_STEPSIZE` | Maximum internal step size for stability | `1e-4` |
| **Function Mapping** | | | |
| | `MODEL_STATE_EQUATION` | Maps to your state equation function | `lorenzAttractor` |
| | `MODEL_OUTPUT_EQUATION` | Maps to your output equation function | `lorenzAttractorOutputs` |
| **Logging (Optional)** | | | |
| | `LOG_INFO` | Category name for info messages | `"INFO"` |
| | `LOG_ERROR` | Category name for error messages | `"ERROR"` |
| | `LOG_WARNING` | Category name for warning messages | `"WARNING"` |

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

---

## How to add your own Integrator

