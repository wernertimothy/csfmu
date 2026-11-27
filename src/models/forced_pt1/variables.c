#include "variables.h"
#include "integrator.h"

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    { PARAM(tau),   .start = 0.5, .unit = "s", .min = SOME(0.001), .max = SOME(100.0), .nominal = 1.0,
      .description = "Time constant of the first-order system" },
    { PARAM(k),     .start = 1.0, .unit = "1", .min = SOME(0.0), .max = SOME(100.0), .nominal = 1.0,
      .description = "Static gain of the input" },
    { PARAM(a),     .start = 1.0, .unit = "1", .nominal = 1.0,
      .description = "Amplitude of the sinusoidal excitation" },
    { PARAM(omega), .start = 6.283185307179586, .unit = "rad/s", .min = SOME(0.0), .nominal = 1.0,
      .description = "Angular frequency of the sinusoidal excitation" },
    { INPUT(u),     .start = 0.0, .unit = "1",
      .description = "Input signal to the first-order system" },
    { OUTPUT(y),                  .unit = "1", .dependsOn = "",
      .description = "Output of the first-order system (equal to state x)" },
    { OUTPUT(excitation),         .unit = "1", .dependsOn = "",
      .description = "Sinusoidal excitation a*sin(omega*t), depends on time only" },
    { STATE(x),     .start = 0.0, .unit = "1",
      .description = "State of the first-order system" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "s", .s = 1 },
    { .name = "1" },
    { .name = "rad/s", .rad = 1, .s = -1 },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "forced_pt1",
    .guid        = "{53c56eff-0173-4bff-8625-cd82b9ba10ad}",
    .description = "First-order lag with time-dependent forcing: tau*dx/dt = -x + k*u + a*sin(omega*t)",
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
    .getStateDerivatives = forcedPt1System,
    .getOutputs          = forcedPt1Outputs,

    /* method for numerical integration */
    .integrator  = &RUNGE_KUTTA_4,
    .maxStepSize = 0.1,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 5.0, .tolerance = 1e-6, .stepSize = 0.01 },
};
