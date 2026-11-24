#ifndef FORCED_PT1_VARIABLES_H
#define FORCED_PT1_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double tau;    /* time constant */
    double k;      /* gain of the input */
    double a;      /* amplitude of the excitation */
    double omega;  /* angular frequency of the excitation */
} Params;

/* inputs */
typedef struct {
    double u;      /* input signal */
} Inputs;

/* outputs */
typedef struct {
    double y;           /* system response */
    double excitation;  /* sinusoidal excitation a*sin(omega*t) */
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x;      /* state of the lag element */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void forcedPt1System(modelInstance* model);

/* output equation */
void forcedPt1Outputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
