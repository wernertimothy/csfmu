
#include <stdlib.h>
#include "model.h"


modelInstance* mallocModel(void) {
    modelInstance* model = malloc(sizeof(modelInstance));
    if (!model) return NULL;

    model->parameter = malloc(sizeof(modelParameter));
    if (!model->parameter) {
        freeModel(model);
        return NULL;
    }

    model->states = malloc(sizeof(modelStates));
    if (!model->states) {
        freeModel(model);
        return NULL;
    }

    model->stateDerivatives = malloc(sizeof(modelStateDerivatives));
    if (!model->stateDerivatives) {
        freeModel(model);
        return NULL;
    }

    model->inputs = malloc(sizeof(modelInputs));
    if (!model->inputs) {
        free(model);
        return NULL;
    }

    model->getStateDerivatives = NULL;
    model->integrate = NULL;

    return model;
}

void freeModel(modelInstance* model) {
    if (model) {
        if (model->parameter) {
            free(model->parameter);
        }
        if (model->states) {
            free(model->states);
        }
        if (model->stateDerivatives) {
            free(model->stateDerivatives);
        }
        if (model->inputs) {
            free(model->inputs);
        }
        free(model);
    }
}

void integrateModel(modelInstance* model, modelInputs* inputs) {
    double dx_dt = (-model->states->x1 + model->parameter->k * inputs->u)/model->parameter->tau;
    model->states->x1 += dx_dt * model->sampleTime;
}

void eulerForward(modelInstance* model) {
    // calculate time derivative dx/dt
    model->getStateDerivatives(model);
    // integrate using simple euler forward
    model->states->x1 += model->stateDerivatives->dx1 * model->sampleTime;
}

void firstOrderSystem(modelInstance* model) {
    double x1 = model->states->x1;
    double u = model->inputs->u;
    double tau = model->parameter->tau;
    double k = model->parameter->k;
    double dx1 = (-x1+k*u)/tau;
    model->stateDerivatives->dx1 = dx1;
}