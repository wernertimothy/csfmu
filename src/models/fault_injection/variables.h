#ifndef FAULT_INJECTION_VARIABLES_H
#define FAULT_INJECTION_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double tau;     /* time constant of the decay */
    double t_fail;  /* time from which on the fault is active */
    bool   fail;    /* switches the fault on */
} Params;

/* outputs */
typedef struct {
    double y;       /* decaying signal */
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x;       /* state of the decay */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void faultInjectionSystem(modelInstance* model);
/* output equation */
void faultInjectionOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
