#include <stdlib.h>
#include <stdio.h>
#include "model.h"
#include "integrator.h"

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

int main() {

    /* begin model config */
    
    /* dimensions */
    size_t nx = 1;       ///> number of states
    size_t nu = 1;       ///> number of inputs
    size_t ny = 1;       ///> number of output
    size_t np = 2;       ///> number of parameter
    
    /* parameter */
    double tau = 0.15;   ///> PT1 time constant
    double k   = 1.0;    ///> PT1 gain
    double x0  = 0.0;    ///> initial condition
    
    /* instantiate model */
    modelInstance* model  = newModel(nx, nu, ny, np);
    
    /* initialize model*/
    model->getStateDerivatives = firstOrderSystem;  ///> system equations
    model->getOutputs          = firstOrderOutputs; ///> y = x
    model->integrate           = eulerForward;      ///> numeric integrator
    model->parameter[0]        = tau;              
    model->parameter[1]        = k;               
    model->states[0]           = x0;
    model->getOutputs(model);
    
    /* begin  simulation */

    /* simualtion config */
    double dt   = 0.01;  ///> sample time
    double T    = 1.0;   ///> final simulation time
    double time = 0.0;   ///> current time of simulation
    
    int N = (int) T/dt;
    for (int k=0; k<N; k++) {
        /* log current state */
        printf("t = %.2f, y = %.2f\n", time, model->outputs[0]);

        /* set model inputs */
        model->inputs[0] = 1.0;

        /* step in time */
        model->getStateDerivatives(model);
        model->integrate(model, dt);
        time += dt;

        /* write outputs */
        model->getOutputs(model);
    }
    /* log current state */
    printf("t = %.2f, y = %.2f\n", time, model->outputs[0]);


    /* free model memory */
    freeModel(model);
    return 0;
}
