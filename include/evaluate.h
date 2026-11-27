/**
 * @file evaluate.h
 * @brief The single entry point for the state derivatives dx/dt = f(t, x, u).
 *
 * Every part of the simulation that needs f calls evaluateDerivatives(),
 * never the model's state equation directly. That way the steps that later
 * tasks add in front of and behind the state equation (an input provider
 * that interpolates the inputs, the solution of algebraic equations, the
 * solution with a mass matrix) apply everywhere f is used.
 *
 * FMI-agnostic: the model layer does not include an FMI header.
 */

#ifndef EVALUATE_H
#define EVALUATE_H

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Computes the state derivatives at the current time, states and inputs.
 *
 * Writes f(model->time, model->states, model->inputs) into
 * model->stateDerivatives by calling the model's state equation.
 *
 * @param model Pointer to the model instance
 *
 * @return SIM_OK. The return value is there for the steps to come, which can
 *         fail: SIM_DISCARD for a numerical failure (e.g. an algebraic
 *         equation that does not converge), SIM_ERROR with the reason in
 *         model->message
 */
simStatus evaluateDerivatives(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
