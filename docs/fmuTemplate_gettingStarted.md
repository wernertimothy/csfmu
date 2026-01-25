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
  | `src/` | Core FMU implementation |
  | `src/models/` | **Model implementations (this is where you will add your model)** |
  | `test/` | Unit and integration tests |

### How to Build the FMU

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
Let's go with the famous *lorenz_attractor* for the sake of this tutorial.


Create new folder with that name under `src/models`, for your model.

You will need to add three files to that folder

| File | Content |
|------|-------------|
|**equations.c** | The differential equations that describe your model |
| **model_config.h** | Mappings, that connect your model with the FMI |
| **modelDescription.xml** | Meta data about your FMU

Let's take a look at these files one by one.

---

### equations.c

In this file you should implement two functions, the models **state equations** and **output equation**.

**State Equation**.
This template assumes that your model can be represented as a potentially nonlinear differential equation, such that the derivatives of the states can be computed by a function **dx=f(t,x,u)**, where **dx** are the state derivatives with respect to time, **t** is the time, **x** are the states and **u** are the inputs to the model.
f is the state equation you have to implement here.
It can have any name.
The signature of the function is
```c
void exampleModelStateEquation(modelInstance* model);
```
and it should calculate all state derivatives and write them to
```c
model->dx[k]
```
for all ```k``` in ```model->info.numberOfStates```.

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
void exampleModelOutputEquation(modelInstance* model);
```
and it should calculate all outputs and write them to
```c
model->outputs[k]
```
for all ```k``` in ```model->info.numberOfOutputs```.

Here is an example implementation that defines all three states of the lorenz attractor as outputs.

```c
void lorenzAttractorOutputs(modelInstance* model) {
  for (int k=0; k<model.info->numberOfStates; k++) {
    model->outputs[k] = model->states[k];
  }
}
```
---

### model_config.h

---

### modelDescription.xml

---

## How to add your own Integrator

