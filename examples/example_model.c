#include <stdlib.h>
#include <stdio.h>
#include "model.h"

int main() {

    /* simualtion condig */
    double dt   = 0.01;  ///> sample time
    double T    = 1.0;   ///> final simulation time
    double time = 0.0;   ///> time of simulation

    /* system dimensions */
    size_t nx = 1;       ///> number of states
    size_t nu = 1;       ///> number of inputs
    size_t np = 2;       ///> number of parameter

    /* instantiate model */
    modelInstance* model  = newModel(nx, nu, np);

    /* initialize model*/
    model->parameter[0]        = 0.15;              ///> PT1 time constant
    model->parameter[1]        = 1.0;               ///> PT1 gain
    model->states[0]           = 0.0;               ///> initial condition
    model->getStateDerivatives = firstOrderSystem;  ///> system equations
    model->integrate           = eulerForward;      ///> numeric integrator

    /* run simulation */
    int N = (int) T/dt;
    for (int k=0; k<N; k++) {
        /* og current state */
        printf("t = %.2f, x = %.2f\n", time, model->states[0]);

        /* set model inputs */
        model->inputs[0] = 1.0;

        /* integrate */
        model->integrate(model, dt);
        time += dt;
    }

    /* free model memory */
    freeModel(model);
    return 0;
}
