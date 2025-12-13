#include <stdio.h>
#include <stdlib.h>
#include "fmi2Functions.h"

/**
 * This is an example of how to use the fmu to run a basic simualtion
 * 
 * The system implemented is a simple first order system:
 * 
 * dx = -1/tau*x + k/tau*u
 * y  = x
 * 
 * the integrator is an explicit wuler forward
 */

 void myLogger(fmi2ComponentEnvironment componentEnvironment,
               fmi2String instanceName,
               fmi2Status status,
               fmi2String category,
               fmi2String message,
               ...) {
    (void)componentEnvironment;
    (void)instanceName;
    (void)status;
    (void)category;
    // simple print
    printf("%s\n", message);
}


int main() {

    /* begin fmu config */
    fmi2String instanceName        = "fmuTemplate";
    fmi2Type   fmuType             = fmi2CoSimulation;
    fmi2String fmuGUID             = "{a1b2c3d4-e5f6-7890-abcd-ef1234567890}";
    fmi2String fmuResourceLocation = "";

    const fmi2CallbackFunctions callbacks = {
        .allocateMemory       = calloc,
        .freeMemory           = free,
        .stepFinished         = NULL,      // only relevant for asynchronous co-Simulation
        .componentEnvironment = NULL,      // no context needed for this example
        .logger               = myLogger
    };

    /* initilaize fmu */
    fmi2Component fmu = fmi2Instantiate(instanceName, fmuType, fmuGUID, fmuResourceLocation, &callbacks, fmi2False, fmi2True);

    /* free fmu memory */
    fmi2FreeInstance(fmu);

    return 0;
}