#include <stdlib.h>
#include <stdio.h>
#include "model.h"

int main() {

    /**
     *  simualtion condig
     */
    double dt   = 0.01;  ///> sample time
    double T    = 1.0;   ///> final simulation time
    double time = 0.0;   ///> time of simulation

    /**
     * system dimensions
     */
    size_t nx = 1;       ///> number of states
    size_t nu = 1;       ///> number of inputs

    /**
     * initialize model
     */
    modelInstance* model  = newModel(nx, nu);
    model->parameter->k   = 1.0;   ///> PT1 gain
    model->parameter->tau = 0.15;  ///> PT1 time constant

    /**
     * set initial condition
     */
    model->states[0] = 0.0;

    /**
     * set model equation
     */
    model->getStateDerivatives = firstOrderSystem;

    /**
     * set integrator
     */
    model->integrate = eulerForward;

    /**
     * run simulationxs
     */
    int N = (int) T/dt;
    for (int k=0; k<N; k++) {
        // log current state
        printf("t = %.2f, x = %.2f\n", time, model->states[0]);

        // set model inputs
        model->inputs[0] = 1.0;

        // integrate
        model->integrate(model, dt);
        time += dt;
    }

    /**
     * free model memory
     */
    freeModel(model);

    return 0;
}
