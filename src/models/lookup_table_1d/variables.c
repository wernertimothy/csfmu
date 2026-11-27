#include "variables.h"
#include "integrator.h"

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    /* a string parameter: the field is char[MODEL_STRING_SIZE], its start value is a text */
    { PARAM(table_file), .startString = "table.csv", .variability = VAR_FIXED,
      .description = "Name of the table file (in the resources of the FMU) with the columns u and y" },
    { INPUT(u),  .start = 0.0, .unit = "1",
      .description = "Value looked up in the table" },
    /* direct feedthrough: y follows u as soon as u is set */
    { OUTPUT(y), .unit = "1", .dependsOn = "u",
      .description = "Table value at u, linearly interpolated and clamped at both ends" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "1" },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "lookup_table_1d",
    .guid        = "{58f777bf-a96c-4daa-b668-f7f3e2574a76}",
    .description = "One-dimensional lookup table without states: y = interp1(table, u)",
    .version     = "1.0.0",

    /* variables and units */
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .units             = UNITS,
    .numberOfUnits     = ARRAY_LEN(UNITS),

    /* system dimensions: no states, so numberOfStates stays 0 */
    .sizeOfParams    = sizeof(Params),
    .sizeOfInputs    = sizeof(Inputs),
    .sizeOfOutputs   = sizeof(Outputs),
    .sizeOfInternals = sizeof(Internals),

    /* model equations */
    .getStateDerivatives = lookupTable1dSystem,
    .getOutputs          = lookupTable1dOutputs,

    /* initialization: reads the table from resources/<table_file> */
    .initialize = lookupTable1dInitialize,

    /* method for numerical integration (there are no states to integrate,
       the FMU only advances the time) */
    .integrator  = &FORWARD_EULER,
    .maxStepSize = 1.0,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.1 },
};
