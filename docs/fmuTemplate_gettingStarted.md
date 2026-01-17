# Welcome to fmuTemplate

Welcome.

Since you have found your way here, let's take a short stroll through the project and get you familiar with the landscape.

## The project



## What you need to know
- know system equations
- know system dimensions
- know which integrator to use
- make sure modelDescription vr matches parameter, input output

## Build a specific model

### Building the FMU

Run the following to build a specific **model_name**, which needs to match a folder name under **src/models**

```bash
rm -rf build
mkdir -p build && cd build
cmake -DMODEL=<model_name> ..
cmake --build .
```

## Add your own Model
