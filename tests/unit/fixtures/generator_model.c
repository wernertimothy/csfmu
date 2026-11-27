/**
 * Model definition for the test of the modelDescription.xml generator.
 *
 * It is not a meaningful model: it only uses every feature of the
 * generator once. The expected result is generator_model.expected.xml.
 */

#include "model.h"
#include "integrator.h"

typedef struct {
    double gain; int mode; bool enabled; double offset; double scaled_gain; int doubled; int operation;
    char file_name[MODEL_STRING_SIZE]; char label[MODEL_STRING_SIZE];
} Params;
typedef struct { double u; double w; bool reset; int select; int command; } Inputs;
typedef struct { double y; double e; double z; int count; bool active; int state; } Outputs;
typedef struct { double rate; int level; bool saturated; double ratio; int direction; } Locals;
typedef struct { double x1; double x2; } States;

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }
static void calculateParameters(modelInstance* m) { (void)m; }
static simStatus initialize(modelInstance* m) { (void)m; return SIM_OK; }

/* enumerations: a gap in the values, descriptions that need escaping, a
 * type used by several variables (written once), negative values */
static const modelEnumerationItem OPERATING_MODE_ITEMS[] = {
    { .name = "off",  .value = 0, .description = "Switched off" },
    { .name = "on",   .value = 1 },
    { .name = "auto", .value = 3, .description = "On if \"needed\" & allowed" },
};
static const modelEnumeration OPERATING_MODE = {
    .name          = "OperatingMode",
    .description   = "Mode: off < on < auto",
    .items         = OPERATING_MODE_ITEMS,
    .numberOfItems = ARRAY_LEN(OPERATING_MODE_ITEMS),
};

static const modelEnumerationItem DIRECTION_ITEMS[] = {
    { .name = "backward", .value = -1 },
    { .name = "forward",  .value = 1 },
};
static const modelEnumeration DIRECTION = {
    .name          = "Direction",
    .items         = DIRECTION_ITEMS,
    .numberOfItems = ARRAY_LEN(DIRECTION_ITEMS),
};

static const modelVariable VARIABLES[] = {
    /* parameters: all types, bounds, nominal, fixed, renamed, undefined unit, enumeration, strings */
    { PARAM(gain),    .start = 2.5, .unit = "N/m", .min = SOME(0.0), .max = SOME(100.0), .nominal = 10.0,
      .description = "Gain: a < b & \"c\" > d" },
    { PARAM(mode),    .start = 3, .min = SOME(0), .max = SOME(5), .variability = VAR_FIXED },
    { PARAM(enabled), .start = 1 },
    { PARAM_AS(offset, "offset_value"), .start = 0.1, .unit = "undefined_unit" },
    { PARAM(operation), .declaredType = &OPERATING_MODE, .start = 3 },
    /* strings: a start text that needs escaping and has UTF-8 ("a" with umlaut), and
     * a tunable, renamed one without start text (start="") */
    { PARAM(file_name), .startString = "data_\xc3\xa4 <v1> & \"final\".csv", .variability = VAR_FIXED,
      .description = "Name of the data file" },
    { PARAM_AS(label, "label_text") },

    /* inputs: all types, enumeration */
    { INPUT(u),      .start = 0.5, .unit = "N/m" },
    { INPUT(w) },
    { INPUT(reset) },
    { INPUT(select), .start = -2 },
    { INPUT(command), .declaredType = &OPERATING_MODE },

    /* outputs: no, some (not in table order) and undeclared dependencies, enumeration */
    { OUTPUT(y),      .dependsOn = "" },
    { OUTPUT(e),      .dependsOn = "w u" },
    { OUTPUT(z) },
    { OUTPUT(count),  .dependsOn = "select" },
    { OUTPUT(active) },
    { OUTPUT(state),  .declaredType = &OPERATING_MODE, .dependsOn = "command" },

    /* locals: all types, bounds, nominal, renamed, enumeration */
    { LOCAL(rate),      .unit = "N/m", .min = SOME(-1.0), .max = SOME(1.0), .nominal = 2.0,
      .description = "Rate of the gain" },
    { LOCAL(level),     .min = SOME(0), .max = SOME(9) },
    { LOCAL(saturated) },
    { LOCAL_AS(ratio, "ratio_value") },
    { LOCAL(direction), .declaredType = &DIRECTION },

    /* states: exact (default) and approx */
    { STATE(x1), .start = 4.0 },
    { STATE_AS(x2, "second_state"), .start = -1.0, .unit = "N/m", .initial = VAR_APPROX },

    /* calculated parameters: tunable and fixed, bounds, renamed */
    { CALCULATED_PARAM(scaled_gain), .unit = "N/m", .min = SOME(0.0), .description = "Gain times 10" },
    { CALCULATED_PARAM_AS(doubled, "doubled_mode"), .variability = VAR_FIXED },
};

static const modelUnit UNITS[] = {
    { .name = "N/m", .kg = 1, .s = -2 },
};

/* a version and a default experiment with all four settings (a start time
 * other than 0); experiments with only some of them are tested by
 * test_defaultExperiment.c */
const modelDefinition MODEL_DEFINITION = {
    .name        = "generator_model",
    .guid        = "{00000000-0000-0000-0000-000000000000}",
    .description = "Fixture for the generator test",
    .version     = "1.0.0",

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
    .integrator          = &EULER_FORWARD,
    .maxStepSize         = 0.01,

    .calculateParameters = calculateParameters,
    .initialize          = initialize,

    .defaultExperiment = { .startTime = 0.5, .stopTime = 10.0, .tolerance = 1e-8, .stepSize = 0.01 },
};
