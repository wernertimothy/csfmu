#ifndef FIRST_ORDER_VARIABLES_H
#define FIRST_ORDER_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double tau;  /* time constant */
    double k;    /* system gain */
} Params;

/* inputs */
typedef struct {
    double u;    /* input signal */
} Inputs;

/* outputs */
typedef struct {
    double y;    /* system response */
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x;    /* state of the lag element */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void firstOrderSystem(modelInstance* model);

/* output equation */
void firstOrderOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
