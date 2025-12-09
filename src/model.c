
#include <stdlib.h>
#include "model.h"

/**
 * Create a new instance of a model
 */
modelInstance* newModel(size_t nx, size_t nu, size_t ny, size_t np) {
    modelInstance* model = malloc(sizeof(modelInstance));
    if (!model) return NULL;
    /* model meta data */
    model->info = malloc(sizeof(modelInfo));
    if (!model->info) {
        free(model);
        return NULL;
    }
    model->info->numberOfParameter = np;
    model->info->numberOfInputs    = nu;
    model->info->numberOfOutputs   = ny;
    model->info->numberOfStates    = nx;
    /* model parameter */
    model->parameter = calloc(model->info->numberOfParameter, sizeof(double));
    if (!model->parameter) {
        freeModel(model);
        return NULL;
    }
    /* model inputs */
    model->inputs = calloc(model->info->numberOfInputs, sizeof(double));
    if (!model->inputs) {
        freeModel(model);
        return NULL;
    }
    /* model outputs */
    model->outputs = calloc(model->info->numberOfOutputs, sizeof(double));
    if (!model->outputs) {
        freeModel(model);
        return NULL;
    }
    /* model states */
    model->states = calloc(model->info->numberOfStates, sizeof(double));
    if (!model->states) {
        freeModel(model);
        return NULL;
    }
    /* model state derivatives */
    model->stateDerivatives = calloc(model->info->numberOfStates, sizeof(double));
    if (!model->stateDerivatives) {
        freeModel(model);
        return NULL;
    }
    /* opaque function pointer */
    model->getStateDerivatives = NULL;
    model->getOutputs          = NULL;
    model->integrate           = NULL;
    return model;
}

/**
 * Free a model instance
 */
void freeModel(modelInstance* model) {
    if (model) {
        if (model->info)             free(model->info);
        if (model->parameter)        free(model->parameter);
        if (model->inputs)           free(model->inputs);
        if (model->outputs)          free(model->outputs);
        if (model->states)           free(model->states);
        if (model->stateDerivatives) free(model->stateDerivatives);
        free(model);
    }
}

/**
 * Euler Forward numeric integrator
 * 
 * TODO: set MINIMUM_STEPSIZE step size and do multiple integration steps
 *       in case that stepSize > MINIMUM_STEPSIZE
 * TODO: implement an integrator.c module with various integrators e.g. RUKU4
 */
void eulerForward(modelInstance* model, double stepSize) {
    /* integrate using simple euler forward */
    for (int k=0; k<model->info->numberOfStates; k++) {
        model->states[k] += model->stateDerivatives[k] * stepSize;
    }
}

/**
 * First Order System state equation
 * TODO: move to specifiv model implementation
 */
void firstOrderSystem(modelInstance* model) {
    /* map states */
    double x1  = model->states[0];
    /* map inputs */
    double u   = model->inputs[0];
    /* map parameter */
    double tau = model->parameter[0];
    double k   = model->parameter[1];
    /* calculate sate differential */
    double dx1 = (-x1+k*u)/tau;
    model->stateDerivatives[0] = dx1;
}

/**
 * First Order System outputs
 * TODO: move to specific model implementation
 */
void firstOrderOutputs(modelInstance* model) {
    model->outputs[0] = model->states[0]; // y = x
}