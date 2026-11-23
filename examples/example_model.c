#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "model.h"
#include "first_order/variables.h"

/**
 * This is an example of how to use the model to run a basic simulation
 * 
 * The system implemented is a simple first order system:
 * 
 * dx = -1/tau*x + k/tau*u
 * y  = x
 * 
 * the integrator is an explicit euler forward
 */

int main(void) {

    /* begin model config */

    /* parameter */
    double tau = 0.15;   ///> PT1 time constant
    double k   = 1.0;    ///> PT1 gain
    double x0  = 0.0;    ///> initial condition

    /* instantiate model (all variables hold their start values) */
    modelInstance* model = newModel(&MODEL_DEFINITION);

    /* access the model variables by name */
    Params*  params  = model->params;
    Inputs*  inputs  = model->inputs;
    Outputs* outputs = model->outputs;
    States*  states  = (States*)model->states;

    /* set parameters and initial conditions, then initialize the model
       (calculated parameters and the model's initialize function, if any) */
    params->tau = tau;
    params->k   = k;
    states->x   = x0;
    if (initializeModel(model) != SIM_OK) {
        fprintf(stderr, "Initialization failed: %s\n", model->message);
        freeModel(model);
        return 1;
    }
    model->getOutputs(model);
    
    /* begin  simulation */

    /* simualtion config */
    double dt   = 0.01;  ///> sample time
    double T    = 1.0;   ///> final simulation time (the current time is model->time)
    
    int N = (int)lround(T / dt);  ///> number of steps
    for (int n = 0; n < N; n++) {
        /* log current state */
        printf("t = %.2f, y = %.2f\n", model->time, outputs->y);

        /* set model inputs */
        inputs->u = 1.0;

        /* step in time (the integrator advances model->time by dt) */
        model->getStateDerivatives(model);
        model->integrate(model, dt);

        /* write outputs */
        model->getOutputs(model);
    }
    /* log current state */
    printf("t = %.2f, y = %.2f\n", model->time, outputs->y);


    /* free model memory */
    freeModel(model);
    return 0;
}
