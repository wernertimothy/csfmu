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

#include "integrator.h"
#include "model.h"

/**
 * Implementation notes for eulerForward():
 * - Explicit (forward) Euler method: x[k+1] = x[k] + dt * f(x[k], u[k])
 * - First-order accuracy: global error is O(dt)
 * - Conditionally stable: step size must satisfy stability condition for the system
 * - State derivatives must be computed (via model->getStateDerivatives()) before calling
 * - Modifies model->states in-place
 * - No internal bounds checking on array access
 */
void eulerForward(modelInstance* model, double stepSize)
{
    /* Integrate all states using explicit Euler formula */
    for (size_t k = 0; k < model->info->numberOfStates; k++) {
        model->states[k] += model->stateDerivatives[k] * stepSize;
    }
}
