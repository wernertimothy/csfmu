#include "variables.h"
#include "integrator.h"

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    { PARAM(tau),    .start = 1.0, .unit = "s", .min = SOME(0.001), .nominal = 1.0,
      .description = "Time constant of the decay" },
    { PARAM(t_fail), .start = 0.55, .unit = "s",
      .description = "Time from which on the state derivative is NaN if fail is true" },
    { PARAM(fail),   .start = 0,
      .description = "Inject the fault (off by default, so the model behaves like a normal one)" },
    { OUTPUT(y),                  .unit = "1", .dependsOn = "",
      .description = "Decaying signal (equal to state x)" },
    { STATE(x),      .start = 1.0, .unit = "1",
      .description = "State of the decay" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "s", .s = 1 },
    { .name = "1" },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "fault_injection",
    .guid        = "{9b01eefc-260a-4f19-a0d8-047012b338b6}",
    .description = "Decay dx/dt = -x/tau whose derivative becomes NaN from t_fail on if fail is set "
                   "(test model for discarded steps)",
    .version     = "1.0.0",

    /* variables and units */
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .units             = UNITS,
    .numberOfUnits     = ARRAY_LEN(UNITS),

    /* system dimensions (no inputs) */
    .sizeOfParams   = sizeof(Params),
    .sizeOfOutputs  = sizeof(Outputs),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    /* model equations */
    .getStateDerivatives = faultInjectionSystem,
    .getOutputs          = faultInjectionOutputs,

    /* method for numerical integration: several sub-steps per communication
     * step of 0.1, so that a fault in the middle of a step shows whether the
     * FMU is rolled back to the communication point */
    .integrator  = &RUNGE_KUTTA_4,
    .maxStepSize = 0.01,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 1.0, .tolerance = 1e-6, .stepSize = 0.1 },
};
