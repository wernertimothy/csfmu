/**
 * @file integrator.h
 * @brief The built-in integration methods and the contract of a step.
 *
 * A model selects its method in its definition:
 *
 *     .integrator  = &RUNGE_KUTTA_4,
 *     .maxStepSize = 1e-3,
 *
 * simulateTo() (simulation.h) cuts every interval into equal steps of at most
 * maxStepSize and calls the method's step() once per step.
 *
 * Contract of step(model, hMax, &hTaken), for the built-in methods and for a
 * descriptor a model brings itself (see docs/csfmu_gettingStarted.md):
 *
 * - On entry model->time is the start t of the step and
 *   model->stateDerivatives holds f(t, x, u): simulateTo() has evaluated it.
 *   model->scratch holds scratchVectors * nx doubles of undefined content.
 * - For every further stage the method sets model->time to the time of the
 *   stage and calls evaluateDerivatives() (evaluate.h), never the model's
 *   state equation directly. If that does not return SIM_OK, step() returns
 *   the same status at once; simulateTo() then restores the model (SIM_DISCARD)
 *   or reports the error (SIM_ERROR).
 * - On return the states and model->time are at t + *hTaken. A fixed-step
 *   method takes the whole step: *hTaken = hMax. simulateTo() checks both, up
 *   to rounding, and fails with SIM_ERROR otherwise. The content of
 *   model->stateDerivatives is undefined: simulateTo() evaluates f again at
 *   the start of the next step.
 *
 * FMI-agnostic: the model layer does not include an FMI header.
 */

#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Explicit (forward) Euler method, order 1.
 *
 *     x(t + h) = x(t) + h * f(t, x)
 *
 * Uses the derivatives simulateTo() evaluated at the start, so a step costs
 * no further evaluation. Conditionally stable: maxStepSize must be small
 * enough for the fastest dynamics of the model. Needs no work memory.
 */
extern const integratorDef EULER_FORWARD;

/**
 * @brief Classic Runge-Kutta method, order 4.
 *
 *     k1 = f(t,       x)
 *     k2 = f(t + h/2, x + h/2 * k1)
 *     k3 = f(t + h/2, x + h/2 * k2)
 *     k4 = f(t + h,   x + h * k3)
 *     x(t + h) = x(t) + h/6 * (k1 + 2*k2 + 2*k3 + k4)
 *
 * k1 is the derivative simulateTo() evaluated at the start, so a step costs
 * three further evaluations, each at its own stage time. Suits most non-stiff
 * systems. Needs 5 vectors of work memory.
 */
extern const integratorDef RUNGE_KUTTA_4;

#ifdef __cplusplus
}
#endif

#endif
