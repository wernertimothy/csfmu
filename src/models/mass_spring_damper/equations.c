/**************************************************************************************************
*                                                                                                 *
* This file is part of fmuTemplate.                                                               *
*                                                                                                 *
* fmuTemplate -- A template for Co-Simulation FMUs written in C.                                  *
* Copyright (C) 2025 Timothy Werner                                                               *
*                                                                                                 *
* SPDX-License-Identifier: MIT                                                                    *
*                                                                                                 *
**************************************************************************************************/

#include "model.h"

/**
 * Spring-Mass-Damper System: dx/dt = f(x, u)
 *
 * Implements the differential equation:
 *     dx/dt = v
 *     dv/dt = (-k*x + -d*v + u) / m
 *
 * where:
 *     m = time constant (parameter[0])
 *     k = spring stiffness (parameter[1])
 *     d = damper coefficient (parameter[2])
 *     u = net applied force on the mass (inputs[0])
 *     x = position of the mass relative to equilibrium (states[0])
 *     v = velocity of the mass (states[1])
 *
 * This represents a second-order-system
 *
 * Implementation notes:
 * - Extracts states/inputs/parameters for readability
 * - Computes derivative inline (no intermediate storage needed)
 * - Writes result directly to stateDerivatives[0]
 */
void massSpringDamper(modelInstance* model)
{
    /* Extract state variable */
    double x = model->states[0]; /* position */
    double v = model->states[1]; /* velocity */

    /* Extract input signal */
    double u = model->inputs[0];

    /* Extract parameters */
    double m = model->parameter[0];  /* mass */
    double k = model->parameter[1];  /* spring stiffness */
    double d = model->parameter[2];  /* damping coeff */

    /* Compute state derivatives: dx/dt = v, dv/dt = (-k*x - d*v + u) / m */
    double dx = v;
    double dv = (-k*x - d*v + u)/m;

    /* Write derivative back to model */
    model->stateDerivatives[0] = dx;
    model->stateDerivatives[1] = dv;
}

/**
 * Mass-Spring-Damper output equation: y = h(x, u)
 *
 * For this system, the output is simply equal to the state:
 *     y = x
 *
 * Implementation notes:
 * - Direct state passthrough (no transformation)
 * - Could be extended to include sensor dynamics or filtering
 */
void massSpringDamperOutputs(modelInstance* model)
{
    /* Output equals state (y = x) */
    model->outputs[0] = model->states[0];
    model->outputs[1] = model->states[1];
}
