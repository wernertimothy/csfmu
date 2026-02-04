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
 * First-order lag system (PT1) state equation: dx/dt = f(x, u)
 *
 * Implements the differential equation:
 *     dx/dt = (-x + k*u) / tau
 *
 * where:
 *     tau = time constant (parameter[0])
 *     k   = system gain (parameter[1])
 *     u   = input signal (inputs[0])
 *     x   = state variable (states[0])
 *
 * This represents a first-order low-pass filter or lag element.
 *
 * Implementation notes:
 * - Extracts states/inputs/parameters for readability
 * - Computes derivative inline (no intermediate storage needed)
 * - Writes result directly to stateDerivatives[0]
 */
void firstOrderSystem(modelInstance* model)
{
    /* Extract state variable */
    double x1 = model->states[0];

    /* Extract input signal */
    double u = model->inputs[0];

    /* Extract parameters */
    double tau = model->parameter[0];  /* Time constant */
    double k = model->parameter[1];    /* System gain */

    /* Compute state derivative: dx/dt = (-x + k*u) / tau */
    double dx1 = (-x1 + k * u) / tau;

    /* Write derivative back to model */
    model->stateDerivatives[0] = dx1;
}

/**
 * First-order system output equation: y = h(x, u)
 *
 * For this system, the output is simply equal to the state:
 *     y = x
 *
 * Implementation notes:
 * - Direct state passthrough (no transformation)
 * - Could be extended to include sensor dynamics or filtering
 */
void firstOrderOutputs(modelInstance* model)
{
    /* Output equals state (y = x) */
    model->outputs[0] = model->states[0];
}
