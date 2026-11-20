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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Performs one integration step using the explicit Euler (forward Euler) method.
 *
 * Advances the model state by one time step using the first-order explicit Euler
 * integration scheme:
 *
 *     x(t + dt) = x(t) + dt * f(t, x)
 *
 * This method is simple and fast but only first-order accurate. It is suitable
 * for non-stiff systems with sufficiently small step sizes. The state derivatives
 * are computed by calling the model's getStateDerivatives function pointer.
 *
 * @param model Pointer to the model instance containing states and derivatives.
 *              Must not be NULL. model->stateDerivatives must hold f at
 *              model->time; on return model->time is advanced by dt.
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

/**
 * @brief Performs one integration step using the classic 4th-order Runge-Kutta method.
 *
 * Advances the model state by one time step using the RK4 scheme:
 *
 *     k1 = f(t,       x)
 *     k2 = f(t + h/2, x + h/2 * k1)
 *     k3 = f(t + h/2, x + h/2 * k2)
 *     k4 = f(t + h,   x + h * k3)
 *     x(t + dt) = x(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
 *
 * This method is fourth-order accurate (global error O(h^4)) and suitable for
 * most non-stiff ODE systems. It requires four evaluations of the state
 * derivative function per step.
 *
 * The function reuses the pre-computed derivatives in model->stateDerivatives
 * as k1, avoiding a redundant evaluation. The remaining three stages (k2, k3,
 * k4) are computed internally by temporarily modifying model->states and
 * setting model->time to the stage time. On return model->time is t + h.
 *
 * @param model Pointer to the model instance containing states and derivatives.
 *              Must not be NULL. The getStateDerivatives function pointer must
 *              be set before calling.
 * @param stepSize Integration time step (dt). Must be positive.
 *
 * @note Uses model->scratch for temporary storage, so no memory is allocated
 *       and the stack usage does not depend on the number of states.
 * @note model->stateDerivatives must be computed before calling (same contract
 *       as eulerForward).
 *
 * @see eulerForward
 * @see modelInstance
 */
void rungeKutta4(modelInstance* model, double stepSize);

#ifdef __cplusplus
}
#endif

#endif