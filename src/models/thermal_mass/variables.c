#include "variables.h"
#include "integrator.h"

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    { PARAM(C),      .start = 1000.0, .unit = "J/K", .min = SOME(0.0), .nominal = 1000.0,
      .variability = VAR_FIXED, .description = "Heat capacity" },
    { PARAM(R),      .start = 0.1,    .unit = "K/W", .min = SOME(0.0), .nominal = 0.1,
      .variability = VAR_FIXED, .description = "Thermal resistance to the ambient" },
    { PARAM(T_amb),  .start = 293.15, .unit = "K",   .min = SOME(0.0), .nominal = 300.0,
      .description = "Ambient temperature" },
    { INPUT(P),      .start = 0.0,    .unit = "W",
      .description = "Heating power" },
    { OUTPUT(T_out),                  .unit = "K",   .dependsOn = "",
      .description = "Temperature of the mass (equal to state T)" },
    /* computed by the output equation, readable but not part of the interface */
    { LOCAL(Q_loss),                  .unit = "W",
      .description = "Heat flow to the ambient Q_loss = (T - T_amb)/R" },
    { STATE(T),      .start = 293.15, .unit = "K",   .min = SOME(0.0), .nominal = 300.0,
      .description = "Temperature of the mass" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "J/K", .kg = 1, .m = 2, .s = -2, .K = -1 },
    { .name = "K/W", .kg = -1, .m = -2, .s = 3, .K = 1 },
    { .name = "K",   .K = 1 },
    { .name = "W",   .kg = 1, .m = 2, .s = -3 },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "thermal_mass",
    .guid        = "{812accd0-21a6-4d42-89a1-5620ced6aaf6}",
    .description = "Heated mass with heat loss to the ambient: C*dT/dt = P - (T - T_amb)/R",
    .version     = "1.0.0",

    /* variables and units */
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .units             = UNITS,
    .numberOfUnits     = ARRAY_LEN(UNITS),

    /* system dimensions */
    .sizeOfParams   = sizeof(Params),
    .sizeOfInputs   = sizeof(Inputs),
    .sizeOfOutputs  = sizeof(Outputs),
    .sizeOfLocals   = sizeof(Locals),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    /* model equations */
    .getStateDerivatives = thermalMassSystem,
    .getOutputs          = thermalMassOutputs,

    /* method for numerical integration (time constant R*C = 100 s) */
    .integrate   = rungeKutta4,
    .maxStepSize = 1.0,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 500.0, .tolerance = 1e-6, .stepSize = 1.0 },
};
