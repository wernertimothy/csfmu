/**
 * Model definition for the test of the modelDescription.xml generator.
 *
 * It is not a meaningful model: it only uses every feature of the
 * generator once. The expected result is generator_model.expected.xml.
 */

#include "model.h"

typedef struct { double gain; int mode; bool enabled; double offset; } Params;
typedef struct { double u; double w; bool reset; int select; } Inputs;
typedef struct { double y; double e; double z; int count; bool active; } Outputs;
typedef struct { double rate; int level; bool saturated; double ratio; } Locals;
typedef struct { double x1; double x2; } States;

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }
static void integrator(modelInstance* m, double stepSize) { (void)m; (void)stepSize; }

static const modelVariable VARIABLES[] = {
    /* parameters: all types, bounds, nominal, fixed, renamed, undefined unit */
    { PARAM(gain),    .start = 2.5, .unit = "N/m", .min = SOME(0.0), .max = SOME(100.0), .nominal = 10.0,
      .description = "Gain: a < b & \"c\" > d" },
    { PARAM(mode),    .start = 3, .min = SOME(0), .max = SOME(5), .variability = VAR_FIXED },
    { PARAM(enabled), .start = 1 },
    { PARAM_AS(offset, "offset_value"), .start = 0.1, .unit = "undefined_unit" },

    /* inputs: all types */
    { INPUT(u),      .start = 0.5, .unit = "N/m" },
    { INPUT(w) },
    { INPUT(reset) },
    { INPUT(select), .start = -2 },

    /* outputs: no, some (not in table order) and undeclared dependencies */
    { OUTPUT(y),      .dependsOn = "" },
    { OUTPUT(e),      .dependsOn = "w u" },
    { OUTPUT(z) },
    { OUTPUT(count),  .dependsOn = "select" },
    { OUTPUT(active) },

    /* locals: all types, bounds, nominal, renamed */
    { LOCAL(rate),      .unit = "N/m", .min = SOME(-1.0), .max = SOME(1.0), .nominal = 2.0,
      .description = "Rate of the gain" },
    { LOCAL(level),     .min = SOME(0), .max = SOME(9) },
    { LOCAL(saturated) },
    { LOCAL_AS(ratio, "ratio_value") },

    /* states */
    { STATE(x1), .start = 4.0 },
    { STATE_AS(x2, "second_state"), .start = -1.0, .unit = "N/m" },
};

static const modelUnit UNITS[] = {
    { .name = "N/m", .kg = 1, .s = -2 },
};

/* no version and no default experiment: both are optional */
const modelDefinition MODEL_DEFINITION = {
    .name        = "generator_model",
    .guid        = "{00000000-0000-0000-0000-000000000000}",
    .description = "Fixture for the generator test",

    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .units             = UNITS,
    .numberOfUnits     = ARRAY_LEN(UNITS),

    .sizeOfParams   = sizeof(Params),
    .sizeOfInputs   = sizeof(Inputs),
    .sizeOfOutputs  = sizeof(Outputs),
    .sizeOfLocals   = sizeof(Locals),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrate           = integrator,
    .maxStepSize         = 0.01,
};
