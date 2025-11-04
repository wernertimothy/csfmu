# Co-Simulation FMUs

## What is an FMU ? 

A **F**unctional **M**ock-up **U**nit is a self-contained model of a system, that has a standardized interface for it's simulation.

Let's break that down.

Every system can be abstracted into a mathematical function that maps *inputs* $u$ in combination with certain *parameters* $p$ through it's *states* $x$ onto some *outputs* $y$.

$$ y = f(x,u,p)$$

In general the function $f$ is a discretized version of a differential equation that models a dynamic process which is part of a physical system or a software component that mimics the outputs of an ECU.
The combination of which is in general the subject of study in a SiL in form of a feedback control system.

An **FMU** is standardized way to define such a component and wrap in a standardized interface for simulation.

## Okay. But why ?

Imagine you want to simulate a complex system that consists of differrent sub-systems which are coupled.
An electric vehicle for example has mechanical components (drivetrain, suspension, vehicle body), electrical components (battery, engine, electronics), thermal components (cooling system), hydraulic components (braking system), control components (ECU, software) and many more.

Each of these sub-systems exist in their own right and have inputs that creat outputs.
Which is to say they are **functional**.
Each sub-system is a *model* of some sort of a process and therefore represents a **mock-up** of the real world which is to be simulated.
and finally each sub-system is one **unit** within a coupled system.

So almost every model of the real world can be broken down into these **Functional Mock-up Units**. 

In general in the industry every component of a system is desgined by different teams, different departments or even different companies.
Every 

## What is Co-Simulation?

## 