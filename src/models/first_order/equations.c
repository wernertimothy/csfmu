#include "model.h"

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
 * First Order System outputs
 */
void firstOrderOutputs(modelInstance* model) {
    model->outputs[0] = model->states[0]; // y = x
}