#ifndef MASS_SPRING_DAMPER_VARIABLES_H
#define MASS_SPRING_DAMPER_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double m;  /* mass */
    double k;  /* spring stiffness */
    double d;  /* damping coefficient */
} Params;

/* inputs */
typedef struct {
    double u;  /* net applied force on the mass */
} Inputs;

/* outputs */
typedef struct {
    double x;  /* position */
    double v;  /* velocity */
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x;  /* position of the mass relative to equilibrium */
    double v;  /* velocity of the mass */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void massSpringDamper(modelInstance* model);
/* output equation */
void massSpringDamperOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
