#include "variables.h"
#include "integrator.h"

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    { PARAM(tau), .start = 0.15, .unit = "s", .min = SOME(0.001), .max = SOME(10.0), .nominal = 1.0,
      .description = "Time constant of the first-order system" },
    { PARAM(k),   .start = 1.0,  .unit = "1", .min = SOME(0.0),   .max = SOME(100.0), .nominal = 1.0,
      .description = "Static gain of the first-order system" },
    { INPUT(u),   .start = 0.0,  .unit = "1",
      .description = "Input signal to the first-order system" },
    { OUTPUT(y),                 .unit = "1", .dependsOn = "",
      .description = "Output of the first-order system (equal to state x)" },
    { STATE(x),   .start = 0.0,  .unit = "1",
      .description = "State of the first-order system" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "s", .s = 1 },
    { .name = "1" },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "first_order",
    .guid        = "{a1b2c3d4-e5f6-7890-abcd-ef1234567890}",
    .description = "First-order lag system (PT1): dx/dt = (-x + k*u) / tau",
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
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    /* model equations */
    .getStateDerivatives = firstOrderSystem,
    .getOutputs          = firstOrderOutputs,

    /* method for numerical integration */
    .integrator  = &EULER_FORWARD,
    .maxStepSize = 1e-3,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.01 },
};
