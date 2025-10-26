#include <stdlib.h>
#include <stdio.h>
#include "model.h"

int main() {

    // simualtion condig
    double sampleTime = 0.01;
    double finalTime  = 1.0;
    double time       = 0.0;

    // set model
    modelInstance* model = mallocModel();
    model->sampleTime            = sampleTime;
    model->parameter->k          = 1.0;        
    model->parameter->tau        = 0.15;
    model->states->x1            = 0.0;
    model->stateDerivatives->dx1 = 0.0;
    model->inputs->u             = 0.0;

    // set model equation
    model->getStateDerivatives = firstOrderSystem;

    // set integrator
    model->integrate = eulerForward;

    // simulation
    int numberOfSteps = (int) finalTime/sampleTime;
    for (int k=0; k<numberOfSteps; k++) {
        // log current state
        printf("t = %.2f, x = %.2f\n", time, model->states->x1);

        // set model inputs
        model->inputs->u = 1.0;

        // integrate
        model->integrate(model);
        time += sampleTime;
    }

    // end
    freeModel(model);

    return 0;
}
