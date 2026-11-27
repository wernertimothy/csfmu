#include "variables.h"
#include "integrator.h"

/*
 * published variables
 *
 * This model shows that variables can be of type double, int or bool and
 * can be declared in any order. The entries are deliberately not grouped
 * by role or type.
 */
static const modelVariable VARIABLES[] = {
    { OUTPUT(limit_reached), .dependsOn = "",
      .description = "True once the ramp has reached the limit" },
    { INPUT(enable),    .start = 1,
      .description = "The ramp runs while this input is true" },
    { PARAM(rate),      .start = 1.0, .unit = "1/s", .min = SOME(0.0),
      .description = "Ramp rate" },
    { STATE(x),         .start = 0.0, .unit = "1",
      .description = "Ramp value" },
    { PARAM(direction), .start = 1, .min = SOME(-1), .max = SOME(1),
      .description = "Direction of the ramp: +1 up, -1 down" },
    { OUTPUT(level),    .dependsOn = "",
      .description = "Ramp value rounded down to an integer" },
    { INPUT(boost),     .start = 1, .min = SOME(0),
      .description = "Multiplier of the ramp rate" },
    { OUTPUT(y),        .unit = "1", .dependsOn = "",
      .description = "Ramp value (limited if clamp is true)" },
    { PARAM(clamp),     .start = 0,
      .description = "Limit the output y to the value of limit" },
    { PARAM(limit),     .start = 5.0, .unit = "1", .variability = VAR_FIXED,
      .description = "Threshold of the ramp (can only be set before the simulation starts)" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "1" },
    { .name = "1/s", .s = -1 },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "typed_demo",
    .guid        = "{d4e5f6a7-b8c9-0123-def0-456789012345}",
    .description = "Ramp with real, integer and boolean variables: dx/dt = enable * rate * direction * boost",
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
    .getStateDerivatives = typedDemoSystem,
    .getOutputs          = typedDemoOutputs,

    /* method for numerical integration */
    .integrator  = &FORWARD_EULER,
    .maxStepSize = 1e-3,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.01 },
};
