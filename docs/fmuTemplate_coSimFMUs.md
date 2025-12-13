# Co-Simulation FMUs
 
## What is an FMU ?
 
A **Functional Mock-up Unit (FMU)** is a self-contained model of a system that provides an interface for simulation, based on an open standard API.
 
Let's break that down.
 
Every system can be abstracted into a mathematical function that maps *inputs* u in combination with certain *parameters* p through its *states* x onto some *outputs* y.
For any dynamic system, i.e. a system that changes in time, this function is a *differential equation*.
To simulate the system is to numerically solve this differential equation.
 
An FMU is a way to define such a function and wrap it in a standardized API to simulate it.
This standard is called the **Functional Mock-up Interface (fmi)**.
 
## Okay. But why ?
 
Imagine you want to simulate a complex system that consists of differrent sub-systems which are coupled.
An electric vehicle for example has:
 
* Mechanical components (drivetrain, suspension, body)
* Electrical components (battery, engine, electronics)
* Thermal components (cooling system)
* Hydraulic components (braking system)
* Control components (ECU, software)
* and many more
 
Each of these sub-systems exist in their own right and has inputs that create outputs.
Which is to say they are **functional**.
Each sub-system is a *model* of some sort of a process and therefore represents a **mock-up** of the real world which is to be simulated.
And finally, each sub-system represents one **unit** within a coupled system.
 
So almost every model of the real world can be broken down into these **Functional Mock-up Units**.
 
In the industry every component of a system is designed by different teams, different departments or even different companies.
Every domain (mechanics, electrics, fluid-dynamics, software, etc.) uses its own tools and laguages.
Once every part has their model ready and it’s time to simulate the entire system, integrating these models becomes a great challenge.
 
Born was the [fmi](https://fmi-standard.org/) by the *Modelica Association* in 2010 to solve this interoperability problem.
 
Today, many development and simulation tools support exporting and importing FMUs, enabling models to be integrated independently of their origin.
 
## Makes sense. And what is Co-Simulation ?
 
Right.
The model is just one half of the equation.
In order for the model to be of any use, it needs to be simulated.
That is the whole purpose of a model.
To *simulate* means to generate *outputs* given certain *inputs*.
In a coupled system, the *outputs* of one FMU are then *inputs* to another FMU (and so on...) and in general there are outputs, that also go "backwards" to previous FMUs, which generates a so called *feedback control loop*.
 
In order to achieve this loop, the individual FMUs have to be integrated and connected within some *integration tool*.
**Co-Simulation** has to do with the philosophy of how the FMUs are integrated.
 
So the second half of the equation is the *integrator*, the thing that tells how a model evolves through time.
In one sentence, an integrator is an implementation of a [numeric calculation of an integral](https://en.wikipedia.org/wiki/Numerical_integration) that solves a differential equation.
I.e. it calculates the solution of a definite integral over the model for a given time interval.
 
In a **Co-Simulation** FMU, this integrator is implemented within the FMU.
So when the integration environment wants to simulate the FMU, it knows the FMU will deliver the results itself.
It only has to tell it to do so via the fmi.

```
┌─────────────────────────────────────┐
│  Integration Tool                   │
│                                     │
│     ┌──────────┐   ┌──────────┐     │
│  ┌─>│ FMU A    │──>│ FMU B    │──┐  │
│  │  │ + Integr.│   │ + Integr.│  │  │
│  │  └──────────┘   └──────────┘  │  │
│  └───────────────────────────────┘  │
│                                     │
└─────────────────────────────────────┘
```
 
The other philosophy is called **Model Exchange**.
As the name implies, here the FMU is "just" the model itself and has no integrator implemented.
When the integration tool wants to simulate the FMU, it knows it has to actually do the integration and can get the needed information via the fmi.
 
## Gotcha. One Last question. Exactly what is an FMU tho ?
 
An FMU is a **ZIP Archive** with the extension *.fmu* that has the following structure

```
MyModel.fmu
│
├── modelDescription.xml          # metadata
│
├── binaries/                     # shared lib
│   ├── win64/
│   │   └── MyModel.dll
│   ├── linux64/
│   │   └── MyModel.so
│   └── darwin64/
│       └── MyModel.dylib
│
├── sources/                      # Optional: source code
│   ├── model.c
│   ├── model.h
│   └── buildDescription.xml
│
└── resources/                    # Optional: additional files
    ├── parameters.txt
    ├── lookup_table.csv
    └── icon.png
```
