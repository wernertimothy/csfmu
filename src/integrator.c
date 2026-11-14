#include "integrator.h"
#include "model.h"
#include <string.h>

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
    for (size_t k = 0; k < model->def->numberOfStates; k++) {
        model->states[k] += model->stateDerivatives[k] * stepSize;
    }
}

/**
 * Implementation notes for rungeKutta4():
 * - Classic 4th-order Runge-Kutta method (RK4):
 *     k1 = f(x)
 *     k2 = f(x + h/2 * k1)
 *     k3 = f(x + h/2 * k2)
 *     k4 = f(x + h * k3)
 *     x[k+1] = x[k] + h/6 * (k1 + 2*k2 + 2*k3 + k4)
 * - Fourth-order accuracy: global error is O(h^4)
 * - Reuses pre-computed derivatives (model->stateDerivatives) as k1
 * - Temporarily modifies model->states for intermediate evaluations
 * - Uses model->scratch for temporary storage (5 vectors of length nx)
 * - A model without states has no storage at all, so it returns early
 * - Modifies model->states in-place with the final result
 */
void rungeKutta4(modelInstance* model, double stepSize)
{
    size_t nx = model->def->numberOfStates;
    if (nx == 0) {
        return;
    }

    double* k1 = model->scratch;
    double* k2 = k1 + nx;
    double* k3 = k2 + nx;
    double* k4 = k3 + nx;
    double* stateBackup = k4 + nx;

    /* k1 = f(x) — reuse already-computed derivatives */
    memcpy(k1, model->stateDerivatives, nx * sizeof(double));
    memcpy(stateBackup, model->states, nx * sizeof(double));

    /* k2 = f(x + h/2 * k1) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + 0.5 * stepSize * k1[i];
    }
    model->getStateDerivatives(model);
    memcpy(k2, model->stateDerivatives, nx * sizeof(double));

    /* k3 = f(x + h/2 * k2) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + 0.5 * stepSize * k2[i];
    }
    model->getStateDerivatives(model);
    memcpy(k3, model->stateDerivatives, nx * sizeof(double));

    /* k4 = f(x + h * k3) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i] + stepSize * k3[i];
    }
    model->getStateDerivatives(model);
    memcpy(k4, model->stateDerivatives, nx * sizeof(double));

    /* x(t+h) = x(t) + h/6 * (k1 + 2*k2 + 2*k3 + k4) */
    for (size_t i = 0; i < nx; i++) {
        model->states[i] = stateBackup[i]
            + (stepSize / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }
}
