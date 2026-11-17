#include "variables.h"
#include "integrator.h"

/*
 * published variables (order is free)
 *
 * The variable table is written in C, also for models whose equations are
 * written in C++ (see equations.cpp).
 *
 * The states are published with the prefix state_ because the names x and
 * theta are already taken by the outputs.
 */
static const modelVariable VARIABLES[] = {
    { PARAM(M),     .start = 1.0,  .unit = "kg",   .min = SOME(0.001), .max = SOME(1000.0), .nominal = 1.0,
      .description = "Cart mass" },
    { PARAM(m),     .start = 0.1,  .unit = "kg",   .min = SOME(0.001), .max = SOME(100.0),  .nominal = 0.1,
      .description = "Pendulum mass" },
    { PARAM(l),     .start = 0.5,  .unit = "m",    .min = SOME(0.01),  .max = SOME(10.0),   .nominal = 0.5,
      .description = "Pendulum length (distance from pivot to center of mass)" },
    { PARAM(g),     .start = 9.81, .unit = "m/s2", .min = SOME(0.0),   .max = SOME(100.0),  .nominal = 9.81,
      .description = "Gravitational acceleration" },
    { INPUT(F),     .start = 0.0,  .unit = "N",
      .description = "Horizontal force applied to the cart" },
    { OUTPUT(x),                   .unit = "m",   .dependsOn = "",
      .description = "Cart position" },
    { OUTPUT(theta),               .unit = "rad", .dependsOn = "",
      .description = "Pendulum angle from vertical (positive = counterclockwise)" },
    { STATE_AS(x, "state_x"),                 .start = 0.0, .unit = "m",
      .description = "State: cart position" },
    { STATE_AS(x_dot, "state_x_dot"),         .start = 0.0, .unit = "m/s",
      .description = "State: cart velocity" },
    { STATE_AS(theta, "state_theta"),         .start = 0.1, .unit = "rad",
      .description = "State: pendulum angle from vertical" },
    { STATE_AS(theta_dot, "state_theta_dot"), .start = 0.0, .unit = "rad/s",
      .description = "State: pendulum angular velocity" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "kg",    .kg = 1 },
    { .name = "m",     .m = 1 },
    { .name = "m/s",   .m = 1, .s = -1 },
    { .name = "m/s2",  .m = 1, .s = -2 },
    { .name = "rad",   .rad = 1 },
    { .name = "rad/s", .rad = 1, .s = -1 },
    { .name = "N",     .kg = 1, .m = 1, .s = -2 },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "pendulum_on_cart",
    .guid        = "{c3d4e5f6-a7b8-9012-cdef-345678901234}",
    .description = "Inverted pendulum on a cart: nonlinear 4th-order system",
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
    .getStateDerivatives = pendulumOnCartSystem,
    .getOutputs          = pendulumOnCartOutputs,

    /* method for numerical integration */
    .integrate   = rungeKutta4,
    .maxStepSize = 1e-3,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.001 },
};
