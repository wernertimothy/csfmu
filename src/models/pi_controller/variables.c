#include "variables.h"
#include "integrator.h"

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    { PARAM(Kp),   .start = 2.0, .unit = "1", .nominal = 1.0,
      .description = "Proportional gain" },
    { PARAM(Ki),   .start = 1.0, .unit = "1/s", .nominal = 1.0,
      .description = "Integral gain" },
    { INPUT(r),    .start = 0.0, .unit = "1",
      .description = "Reference (setpoint)" },
    { INPUT(y_m),  .start = 0.0, .unit = "1",
      .description = "Measured plant output" },
    /* direct feedthrough: both outputs use the inputs in the output equation */
    { OUTPUT(u),                 .unit = "1", .dependsOn = "r y_m",
      .description = "Control signal u = Kp*e + Ki*x_I" },
    { OUTPUT(e),                 .unit = "1", .dependsOn = "r y_m",
      .description = "Control error e = r - y_m" },
    { STATE(x_I),  .start = 0.0, .unit = "s",
      .description = "Integral of the control error" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "1" },
    { .name = "1/s", .s = -1 },
    { .name = "s", .s = 1 },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "pi_controller",
    .guid        = "{a48fb924-55f0-4b6e-bd68-63cd623efda7}",
    .description = "PI controller with direct feedthrough: e = r - y_m, dx_I/dt = e, u = Kp*e + Ki*x_I",
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
    .getStateDerivatives = piControllerSystem,
    .getOutputs          = piControllerOutputs,

    /* method for numerical integration (exact here: with the inputs held
     * constant over a step, x_I is linear in time) */
    .integrate   = rungeKutta4,
    .maxStepSize = 0.01,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.01 },
};
