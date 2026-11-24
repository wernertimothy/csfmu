#ifndef PENDULUM_ON_CART_VARIABLES_H
#define PENDULUM_ON_CART_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double M;  /* cart mass */
    double m;  /* pendulum mass */
    double l;  /* pendulum length */
    double g;  /* gravity */
} Params;

/* inputs */
typedef struct {
    double F;  /* horizontal force on cart */
} Inputs;

/* outputs */
typedef struct {
    double x;      /* cart position */
    double theta;  /* pendulum angle */
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x;          /* cart position */
    double x_dot;      /* cart velocity */
    double theta;      /* pendulum angle from vertical */
    double theta_dot;  /* angular velocity */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void pendulumOnCartSystem(modelInstance* model);

/* output equation */
void pendulumOnCartOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
