#include <string.h>
#include "integrator.h"
#include "evaluate.h"

/**
 * Implementation notes for the explicit Euler step:
 * - x[k+1] = x[k] + h * f(t[k], x[k], u[k]), with the derivatives that
 *   simulateTo() evaluated at the start of the step
 * - First-order accuracy: global error is O(h)
 * - Conditionally stable: the step size must satisfy the stability condition
 *   of the system
 */
static simStatus eulerForwardStep(modelInstance* model, double hMax, double* hTaken)
{
    double t0 = model->time;

    for (size_t k = 0; k < model->def->numberOfStates; k++) {
        model->states[k] += model->stateDerivatives[k] * hMax;
    }

    model->time = t0 + hMax;
    *hTaken = hMax;
    return SIM_OK;
}

/**
 * Implementation notes for the classic Runge-Kutta step (RK4):
 * - k1 = f(t, x) is the derivative simulateTo() evaluated at the start; the
 *   stages k2, k3 and k4 are evaluated with evaluateDerivatives() at their
 *   own stage times (t + h/2, t + h/2, t + h), so that time-varying models
 *   keep the 4th order
 * - Fourth-order accuracy: global error is O(h^4)
 * - Work memory: 5 vectors of length nx in model->scratch (k1 ... k4 and
 *   the states at the start); a model without states has none and only
 *   advances the time
 * - A failed stage evaluation returns its status at once; the states then
 *   hold an intermediate value, and simulateTo() restores or keeps them
 */
static simStatus rungeKutta4Step(modelInstance* model, double hMax, double* hTaken)
{
    double t0 = model->time;
    size_t nx = model->def->numberOfStates;
    *hTaken = hMax;
    if (nx == 0) {
        model->time = t0 + hMax;
        return SIM_OK;
    }

    double* k1 = model->scratch;
    double* k2 = k1 + nx;
    double* k3 = k2 + nx;
    double* k4 = k3 + nx;
    double* stateBackup = k4 + nx;
    simStatus status;

    /* k1 = f(t, x), evaluated by simulateTo() */
    memcpy(k1, model->stateDerivatives, nx * sizeof(double));
    memcpy(stateBackup, model->states, nx * sizeof(double));

    /* k2 = f(t + h/2, x + h/2 * k1) */
    model->time = t0 + 0.5 * hMax;
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + 0.5 * hMax * k1[i];
    }
    if ((status = evaluateDerivatives(model)) != SIM_OK) {
        return status;
    }
    memcpy(k2, model->stateDerivatives, nx * sizeof(double));

    /* k3 = f(t + h/2, x + h/2 * k2) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + 0.5 * hMax * k2[i];
    }
    if ((status = evaluateDerivatives(model)) != SIM_OK) {
        return status;
    }
    memcpy(k3, model->stateDerivatives, nx * sizeof(double));

    /* k4 = f(t + h, x + h * k3) */
    model->time = t0 + hMax;
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + hMax * k3[i];
    }
    if ((status = evaluateDerivatives(model)) != SIM_OK) {
        return status;
    }
    memcpy(k4, model->stateDerivatives, nx * sizeof(double));

    /* x(t + h) = x(t) + h/6 * (k1 + 2*k2 + 2*k3 + k4) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i]
            + (hMax / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }
    return SIM_OK;
}

const integratorDef EULER_FORWARD = {
    .name           = "Euler forward",
    .step           = eulerForwardStep,
    .scratchVectors = 0,
    .order          = 1,
};

const integratorDef RUNGE_KUTTA_4 = {
    .name           = "Runge-Kutta 4",
    .step           = rungeKutta4Step,
    .scratchVectors = 5,
    .order          = 4,
};
