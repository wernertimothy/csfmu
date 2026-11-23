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
    { PARAM(init_mode), .start = THERMAL_MASS_FIXED_START, .min = SOME(1), .max = SOME(2),
      .variability = VAR_FIXED,
      .description = "Initialization: 1 = fixed_start (T starts at its start value), "
                     "2 = steady_state (T starts at T_amb + R*P)" },
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
    .guid        = "{9c6b82e6-012b-4480-8b82-e860f1f54027}",
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
