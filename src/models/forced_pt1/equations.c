#include <math.h>
#include "variables.h"

/**
 * Forced first-order lag state equation: dx/dt = f(t, x, u)
 *
 * Implements the differential equation:
 *     tau * dx/dt = -x + k*u + a*sin(omega*t)
 *
 * where:
 *     tau   = time constant (parameter)
 *     k     = gain of the input (parameter)
 *     a     = amplitude of the excitation (parameter)
 *     omega = angular frequency of the excitation (parameter)
 *     u     = input signal (input)
 *     x     = state variable (state)
 *     t     = time (model->time)
 *
 * Implementation notes:
 * - The right-hand side depends explicitly on time. The integrator sets
 *   model->time to the time of every stage, so RK4 keeps its 4th order
 */
void forcedPt1System(modelInstance* model)
{
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    const States* x  = (const States*)model->states;
    States*       dx = (States*)model->stateDerivatives;
    double        t  = model->time;

    /* Compute state derivative: dx/dt = (-x + k*u + a*sin(omega*t)) / tau */
    dx->x = (-x->x + p->k * in->u + p->a * sin(p->omega * t)) / p->tau;
}

/**
 * Forced first-order lag output equation: y = h(t, x, u)
 *
 * The outputs are:
 *     y          = x
 *     excitation = a*sin(omega*t)
 *
 * Implementation notes:
 * - No direct feedthrough of u, hence .dependsOn = "" in variables.c
 * - The excitation depends on time only, so it shows whether the outputs
 *   are evaluated at the communication point
 */
void forcedPt1Outputs(modelInstance* model)
{
    const Params* p   = model->params;
    const States* x   = (const States*)model->states;
    Outputs*      out = model->outputs;
    double        t   = model->time;

    out->y          = x->x;
    out->excitation = p->a * sin(p->omega * t);
}
