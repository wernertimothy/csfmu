
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
    model->info->numberOfStates    = nx;
    model->info->numberOfInputs    = nu;
    model->info->numberOfOutputs   = ny;
    model->info->numberOfParameter = np;
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
    /* model inputs */
    model->inputs = calloc(model->info->numberOfInputs, sizeof(double));
    if (!model->inputs) {
        freeModel(model);
        return NULL;
    }
    /* model parameter */
    model->parameter = calloc(model->info->numberOfParameter, sizeof(double));
    if (!model->parameter) {
        freeModel(model);
        return NULL;
    }
    /* model parameter */
    model->outputs = calloc(model->info->numberOfOutputs, sizeof(double));
    if (!model->outputs) {
        freeModel(model);
        return NULL;
    }
    /* model state equation */
    model->getStateDerivatives = NULL;
    /* numeric integrator */
    model->integrate = NULL;
    return model;
}

/**
 * Free a model instance
 */
void freeModel(modelInstance* model) {
    if (model) {
        if (model->info)             free(model->info);
        if (model->states)           free(model->states);
        if (model->stateDerivatives) free(model->stateDerivatives);
        if (model->inputs)           free(model->inputs);
        if (model->outputs)          free(model->outputs);
        if (model->parameter)        free(model->parameter);
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
 * First Order System full time step
 * 
 * note: set inputs before calling this
 * 
 * TODO: Is it cleaner to set inputs within this and make inputs an argument to this function ?
 */
void modelDoTimeStep(modelInstance* model, double stepSize) {
    /* set inputs ? */

    /* calculate time derivative dx/dt */
    model->getStateDerivatives(model);

    /* integrate state differential */
    model->integrate(model, stepSize);

    /* write outputs  */
    model->outputs[0] = model->states[0];
}