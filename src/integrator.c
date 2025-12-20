
#include "integrator.h"
#include "model.h"

/**
 * Euler Forward numeric integrator
 */
void eulerForward(modelInstance* model, double stepSize) {
    /* integrate using simple euler forward */
    for (int k=0; k<model->info->numberOfStates; k++) {
        model->states[k] += model->stateDerivatives[k] * stepSize;
    }
}