#include "variables.h"
#include "integrator.h"

/*
 * published variables (order is free)
 *
 * The states are published as state_x and state_v because the names x and v
 * are already taken by the outputs.
 */
static const modelVariable VARIABLES[] = {
    { PARAM(m),   .start = 1.0, .unit = "kg",    .min = SOME(0.001), .max = SOME(1000.0),  .nominal = 1.0,
      .description = "Mass" },
    { PARAM(k),   .start = 1.0, .unit = "N/m",   .min = SOME(0.0),   .max = SOME(10000.0), .nominal = 1.0,
      .description = "Spring stiffness" },
    { PARAM(d),   .start = 1.0, .unit = "N.s/m", .min = SOME(0.0),   .max = SOME(10000.0), .nominal = 1.0,
      .description = "Damping coefficient" },
    { INPUT(u),   .start = 0.0, .unit = "N",
      .description = "External force applied to the mass" },
    { OUTPUT(x),                .unit = "m",   .dependsOn = "",
      .description = "Position of the mass relative to equilibrium" },
    { OUTPUT(v),                .unit = "m/s", .dependsOn = "",
      .description = "Velocity of the mass" },
    { STATE_AS(x, "state_x"), .start = 0.0, .unit = "m",
      .description = "State: position of the mass relative to equilibrium" },
    { STATE_AS(v, "state_v"), .start = 0.0, .unit = "m/s",
      .description = "State: velocity of the mass" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "kg",    .kg = 1 },
    { .name = "m",     .m = 1 },
    { .name = "m/s",   .m = 1, .s = -1 },
    { .name = "N",     .kg = 1, .m = 1, .s = -2 },
    { .name = "N/m",   .kg = 1, .s = -2 },
    { .name = "N.s/m", .kg = 1, .s = -1 },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "mass_spring_damper",
    .guid        = "{b2c3d4e5-f6a7-8901-bcde-f23456789012}",
    .description = "Mass-spring-damper system: m*x'' + d*x' + k*x = u",
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
    .getStateDerivatives = massSpringDamper,
    .getOutputs          = massSpringDamperOutputs,

    /* method for numerical integration */
    .integrator  = &RUNGE_KUTTA_4,
    .maxStepSize = 1e-3,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.001 },
};
