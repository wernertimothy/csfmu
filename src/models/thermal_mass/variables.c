#include "variables.h"
#include "integrator.h"

/* how T is initialized (enumeration type of the parameter init_mode) */
static const modelEnumerationItem INIT_MODE_ITEMS[] = {
    { .name = "fixed_start",  .value = THERMAL_MASS_FIXED_START,
      .description = "T starts at its start value" },
    { .name = "steady_state", .value = THERMAL_MASS_STEADY_STATE,
      .description = "T starts in equilibrium: T = T_amb + R*P" },
};

static const modelEnumeration INIT_MODE = {
    .name          = "InitMode",
    .description   = "Initialization of the temperature",
    .items         = INIT_MODE_ITEMS,
    .numberOfItems = ARRAY_LEN(INIT_MODE_ITEMS),
};

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    { PARAM(C),      .start = 1000.0, .unit = "J/K", .min = SOME(0.0), .nominal = 1000.0,
      .variability = VAR_FIXED, .description = "Heat capacity" },
    { PARAM(R),      .start = 0.1,    .unit = "K/W", .min = SOME(0.0), .nominal = 0.1,
      .variability = VAR_FIXED, .description = "Thermal resistance to the ambient" },
    { PARAM(T_amb),  .start = 293.15, .unit = "K",   .min = SOME(0.0), .nominal = 300.0,
      .description = "Ambient temperature" },
    { PARAM(init_mode), .declaredType = &INIT_MODE, .start = THERMAL_MASS_FIXED_START,
      .variability = VAR_FIXED, .description = "How the temperature T is initialized" },
    /* computed from R and C by thermalMassParameters, readable but not settable */
    { CALCULATED_PARAM(tau), .unit = "s", .min = SOME(0.0), .nominal = 100.0,
      .variability = VAR_FIXED, .description = "Time constant tau = R*C" },
    { INPUT(P),      .start = 0.0,    .unit = "W",
      .description = "Heating power" },
    { OUTPUT(T_out),                  .unit = "K",   .dependsOn = "",
      .description = "Temperature of the mass (equal to state T)" },
    /* computed by the output equation, readable but not part of the interface */
    { LOCAL(Q_loss),                  .unit = "W",
      .description = "Heat flow to the ambient Q_loss = (T - T_amb)/R" },
    /* initial="approx": in steady_state mode the initialization replaces the start value */
    { STATE(T),      .start = 293.15, .unit = "K",   .min = SOME(0.0), .nominal = 300.0,
      .initial = VAR_APPROX, .description = "Temperature of the mass" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "J/K", .kg = 1, .m = 2, .s = -2, .K = -1 },
    { .name = "K/W", .kg = -1, .m = -2, .s = 3, .K = 1 },
    { .name = "K",   .K = 1 },
    { .name = "W",   .kg = 1, .m = 2, .s = -3 },
    { .name = "s",   .s = 1 },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "thermal_mass",
    .guid        = "{05fad090-5e1b-4491-8171-703ba54eb4b4}",
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

    /* calculated parameters and initialization */
    .calculateParameters = thermalMassParameters,
    .initialize          = thermalMassInitialize,

    /* method for numerical integration (time constant R*C = 100 s) */
    .integrate   = rungeKutta4,
    .maxStepSize = 1.0,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 500.0, .tolerance = 1e-6, .stepSize = 1.0 },
};
