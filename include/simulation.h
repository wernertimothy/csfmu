/**
 * @file simulation.h
 * @brief Advances a model instance in time: the FMI-agnostic orchestrator.
 *
 * simulateTo() is what a co-simulation step does inside the FMU: it cuts the
 * interval into integration steps, evaluates the state derivatives, calls
 * the integrator, checks the result and evaluates the outputs at the end.
 * The FMI wrapper only validates the arguments of the master and maps the
 * result to an FMI status; a standalone simulation calls simulateTo()
 * directly (see examples/example_model.c).
 *
 * The model layer does not include an FMI header.
 */

#ifndef SIMULATION_H
#define SIMULATION_H

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Simulates a model instance from its current time to tEnd.
 *
 * Starts at model->time with the current states, inputs and parameters (the
 * inputs are held constant) and integrates to tEnd:
 *
 * - The interval [t, tEnd] is cut into the smallest number N of equal
 *   integration steps that are not larger than the model's maxStepSize, so
 *   h = (tEnd - t) / N. Step k starts at t + k*h, computed from the
 *   integer k, so rounding does not add up. If tEnd equals t (a step so
 *   short that t + h rounds to t), no integration step is taken.
 * - Before every integration step the state derivatives are evaluated at
 *   its start (evaluateDerivatives()), then the step() of the model's
 *   integrator advances the states. It must take the whole step and leave
 *   model->time at its end, up to rounding (the contract in integrator.h).
 * - At the end model->time is exactly tEnd, and the outputs and local
 *   variables are evaluated there.
 *
 * The model data at the start are saved to model->stepStart, which was
 * allocated with the instance, so nothing is allocated here.
 *
 * The model keeps the time (model->time), so the caller needs no clock of
 * its own. There are two ways to call simulateTo():
 *
 *     simulateTo(model, model->time + h);    // simulate for h from now
 *     simulateTo(model, t0 + (n + 1) * h);   // step n of a fixed grid
 *
 * The first form adds up the step sizes, so over many calls the clock drifts
 * away from the grid by rounding: after 10^7 steps of 1e-4 it is at
 * 999.9999998206155 instead of 1000 (1.8e-10 relative). The second form
 * computes every end from the integer n and lands exactly on the grid. Use
 * it for long loops and whenever the time must hit given points (output
 * times, events, a stop time). An FMU step is of this kind: fmi2DoStep
 * passes currentCommunicationPoint + communicationStepSize, because the
 * master defines the communication points.
 *
 * @param model Pointer to the model instance
 * @param tEnd  End time; finite and not before model->time
 *
 * @return
 * - SIM_OK: the model is at tEnd, its outputs and local variables included.
 * - SIM_DISCARD: a state, a Real output or a Real local variable became NaN
 *   or infinite, or an integration step reported a numerical failure. The
 *   model is restored to its start (time, states, outputs, internals, ...:
 *   all model data, bit for bit), and model->message says what failed and
 *   when. The caller may retry, e.g. with a shorter step.
 * - SIM_ERROR: the call or the model is invalid (tEnd before the start or
 *   not finite, no integrator, maxStepSize not greater than 0, too many
 *   integration steps, an integrator that breaks its contract or reports an
 *   error); model->message has the reason.
 *   The model keeps the values it has at that point, for diagnosis: it is
 *   not restored.
 */
simStatus simulateTo(modelInstance* model, double tEnd);

/**
 * @brief Rounding allowance for two values of a time t.
 *
 * A few units in the last place of t: two computations of the same point in
 * time that differ by less are equal up to rounding.
 *
 * @param time The time t
 *
 * @return 8 * DBL_EPSILON * max(1, |t|)
 */
double timeRoundingTolerance(double time);

#ifdef __cplusplus
}
#endif

#endif
