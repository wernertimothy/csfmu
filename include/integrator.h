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

/**
 * @file integrator.h
 * @brief Numerical integration methods for FMU model state propagation.
 *
 * This module provides numerical integrators for advancing the state of a model
 * instance through time. Integrators compute the next state values based on
 * state derivatives and a time step.
 */

#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "model.h"

/**
 * @brief Performs one integration step using the explicit Euler (forward Euler) method.
 *
 * Advances the model state by one time step using the first-order explicit Euler
 * integration scheme:
 *
 *     x(t + dt) = x(t) + dt * dx/dt
 *
 * This method is simple and fast but only first-order accurate. It is suitable
 * for non-stiff systems with sufficiently small step sizes. The state derivatives
 * are computed by calling the model's getStateDerivatives function pointer.
 *
 * @param model Pointer to the model instance containing states and derivatives.
 *              Must not be NULL.
 * @param stepSize Integration time step (dt). Must be positive.
 *
 * @note This method is conditionally stable. For stability, the step size must
 *       satisfy the CFL condition for the specific system being integrated.
 * @warning For stiff systems, this method may require impractically small step
 *          sizes to maintain stability.
 *
 * @see modelInstance
 */
void eulerForward(modelInstance* model, double stepSize);

#endif