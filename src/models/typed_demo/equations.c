#include <math.h>
#include "variables.h"

/**
 * Typed demo state equation: dx/dt = f(t, x, u)
 *
 * Implements a ramp that can be switched on and off:
 *     dx/dt = rate * direction * boost    if enable
 *     dx/dt = 0                           otherwise
 *
 * where:
 *     rate      = ramp rate (real parameter)
 *     direction = +1 or -1 (integer parameter)
 *     boost     = multiplier of the ramp rate (integer input)
 *     enable    = ramp runs while true (boolean input)
 *     x         = ramp value (state)
 *
 * The purpose of this model is to show the use of integer and boolean
 * variables. They are plain int and bool fields of the structs.
 */
void typedDemoSystem(modelInstance* model)
{
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    States*       dx = (States*)model->stateDerivatives;

    dx->x = in->enable ? p->rate * p->direction * in->boost : 0.0;
}

/**
 * Typed demo output equation: y = h(t, x, u)
 *
 *     limit_reached = x >= limit
 *     level         = floor(x)
 *     y             = min(x, limit)   if clamp
 *     y             = x               otherwise
 */
void typedDemoOutputs(modelInstance* model)
{
    const Params* p   = model->params;
    const States* x   = (const States*)model->states;
    Outputs*      out = model->outputs;

    out->limit_reached = (x->x >= p->limit);
    out->level         = (int)floor(x->x);
    out->y             = (p->clamp && out->limit_reached) ? p->limit : x->x;
}
