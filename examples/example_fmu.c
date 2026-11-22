#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "fmi2Functions.h"

/**
 * This is an example of how to use the fmu to run a basic simulation
 * 
 * The system implemented is a simple first order system:
 * 
 * dx = -1/tau*x + k/tau*u
 * y  = x
 * 
 * the integrator is an explicit euler forward
 */

 void myLogger(fmi2ComponentEnvironment componentEnvironment,
               fmi2String instanceName,
               fmi2Status status,
               fmi2String category,
               fmi2String message,
               ...) {
    (void)componentEnvironment;
    (void)status;

    /* handle variable format specifiers */
    va_list args;
    va_start(args, message);

    /* print message */
    printf("[%s] %s : ", category, instanceName);
    vprintf(message, args);
    printf("\n");

    va_end(args);
}


int main(void) {

    /* simualtion config */
    double dt        = 0.01;  ///> sample time
    double T         = 1.0;   ///> final simulation time
    double startTime = 0.0;   ///> start time of the simulation

    /* begin fmu config */
    fmi2String instanceName        = "csfmu";
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

    /* instantiate fmu */
    fmi2Component fmu = fmi2Instantiate(instanceName, fmuType, fmuGUID, fmuResourceLocation, &callbacks, fmi2False, fmi2True);
    if (!fmu) goto cleanup;

    /* initialize fmu */
    fmi2Status fmuStatus = fmi2SetupExperiment(fmu, fmi2False, 0.0, startTime, fmi2True, T);
    if (fmuStatus >= fmi2Error) goto cleanup;

    fmuStatus = fmi2EnterInitializationMode(fmu);
    if (fmuStatus >= fmi2Error) goto cleanup;

    fmuStatus = fmi2ExitInitializationMode(fmu);
    if (fmuStatus >= fmi2Error) goto cleanup;

    /* begin simulation */
    double y = 0.0;
    double u = 1.0;
    /* value reference = position of the variable in the VARIABLES table of
     * src/models/first_order/variables.c (see also build/modelDescription.xml) */
    const fmi2ValueReference VR_u = 2;
    const fmi2ValueReference VR_y = 3;

    /* deactivate logging */
    fmuStatus = fmi2SetDebugLogging(fmu, fmi2False, 0, NULL);
    if (fmuStatus >= fmi2Error) goto cleanup;

    int N = (int)lround(T / dt);  ///> number of steps
    for (int n = 0; n < N; n++) {
        /* communication point, computed from n so that rounding errors do not accumulate */
        double time = startTime + n * dt;

        /* log current state */
        fmuStatus = fmi2GetReal(fmu, &VR_y, 1, &y);
        if (fmuStatus >= fmi2Error) goto cleanup;

        printf("t = %.2f, y = %.2f\n", time, y);

        /* set fmu input */
        fmuStatus = fmi2SetReal(fmu, &VR_u, 1, &u);
        if (fmuStatus >= fmi2Error) goto cleanup;

        /* step in time */
        fmuStatus = fmi2DoStep(fmu, time, dt, fmi2False);
        if (fmuStatus >= fmi2Error) goto cleanup;
    }
    fmuStatus = fmi2GetReal(fmu, &VR_y, 1, &y);
    if (fmuStatus >= fmi2Error) goto cleanup;

    /* the FMU owns the clock: the end of the last step is its last successful time */
    fmi2Real endTime = 0.0;
    fmuStatus = fmi2GetRealStatus(fmu, fmi2LastSuccessfulTime, &endTime);
    if (fmuStatus >= fmi2Error) goto cleanup;
    printf("t = %.2f, y = %.2f\n", endTime, y);

    /* activate logging */
    fmuStatus = fmi2SetDebugLogging(fmu, fmi2True, 0, NULL);
    if (fmuStatus >= fmi2Error) goto cleanup;

    /* terminate */
    fmuStatus = fmi2Terminate(fmu);
    if (fmuStatus >= fmi2Error) goto cleanup;

    /* free fmu memory */
    cleanup:
        fmi2FreeInstance(fmu);
        return 0;
}