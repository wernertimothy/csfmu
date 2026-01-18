# Welcome to fmuTemplate

**Welcome.**

This overview will give you all information necessary to start implementing you own model equations, compile it to an FMU and test it.

Before we begin, let's take a short stroll through the project and get you familiar with the landscape.

## The Folders

The most important files are `src/fmi2Functions.c`, which implements the FMU functions and everything that is inside the `src/model/your_model` folder, which implements the model.
The rest simply exists to help build an FMU.
Let's have a look...

* `docs`- contains references and helpful information about FMUs in general, the implemented fmi interfaces and inforamtion about this project in general.
* `examples` - contains example implementations that will help you study to code and how to use it
* `external` - contains the fmi2 and fmi3 interface, as well as unity for testing. This is third party code and not implemented by me. The only reason they exist here is because they are a necessary dependency and I want this priject to be self-contained.
* `include` - contains header files
* `scripts`- contains python helper scripts
* `src`- contains the source files
* `src/models` - contains folders for each model implementation. This is where you will add your custome code, i.e. the model you want to simulate.
* `test`- contains unit and integration tests using unity and fmpy 

### How to Build the FMU

Run the following to build a specific **model_name**, which needs to match a folder name under `src/models`

```bash
rm -rf build
mkdir -p build && cd build
cmake -DMODEL=<model_name> ..
cmake --build .
```

## How to add your own Model
