
#include <stdlib.h>
#include "model.h"

/**
 * Create a new instance of a model
 */
modelInstance* newModel(size_t nx, size_t nu) {
    modelInstance* model = malloc(sizeof(modelInstance));
    if (!model) return NULL;
    
    model->info = malloc(sizeof(modelInfo));
    if (!model->info) {
        free(model);
        return NULL;
    }
    model->info->numberOfStates = nx;
    model->info->numberOfInputs = nu;

    model->parameter = malloc(sizeof(modelParameter));
    if (!model->parameter) {
        freeModel(model);
        return NULL;
    }
    
    model->states = calloc(model->info->numberOfStates, sizeof(double));
    if (!model->states) {
        freeModel(model);
        return NULL;
    }
    
    model->stateDerivatives = calloc(model->info->numberOfStates, sizeof(double));
    if (!model->stateDerivatives) {
        freeModel(model);
        return NULL;
    }
    
    model->inputs = calloc(model->info->numberOfInputs, sizeof(double));
    if (!model->inputs) {
        freeModel(model);
        return NULL;
    }
    
    model->getStateDerivatives = NULL;
    model->integrate = NULL;

    return model;
}

void freeModel(modelInstance* model) {
    if (model) {
        if (model->parameter) free(model->parameter);
        if (model->info) free(model->info);
        if (model->states) free(model->states);
        if (model->stateDerivatives) free(model->stateDerivatives);
        if (model->inputs) free(model->inputs);
        free(model);
    }
}

void eulerForward(modelInstance* model, double stepSize) {
    // calculate time derivative dx/dt
    model->getStateDerivatives(model);
    // integrate using simple euler forward
    for (int k=0; k<model->info->numberOfStates; k++) {
        model->states[k] += model->stateDerivatives[k] * stepSize;
    }
}

void firstOrderSystem(modelInstance* model) {
    double x1  = model->states[0];
    double u   = model->inputs[0];
    double tau = model->parameter->tau;
    double k   = model->parameter->k;
    double dx1 = (-x1+k*u)/tau;
    model->stateDerivatives[0] = dx1;
}