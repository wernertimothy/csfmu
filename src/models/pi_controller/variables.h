#ifndef PI_CONTROLLER_VARIABLES_H
#define PI_CONTROLLER_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double Kp;   /* proportional gain */
    double Ki;   /* integral gain */
} Params;

/* inputs */
typedef struct {
    double r;    /* reference (setpoint) */
    double y_m;  /* measured plant output */
} Inputs;

/* outputs */
typedef struct {
    double u;    /* control signal */
    double e;    /* control error r - y_m */
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x_I;  /* integral of the control error */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void piControllerSystem(modelInstance* model);

/* output equation */
void piControllerOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
