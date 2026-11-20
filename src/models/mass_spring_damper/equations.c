#include "variables.h"

/**
 * Spring-Mass-Damper System: dx/dt = f(t, x, u)
 *
 * Implements the differential equation:
 *     dx/dt = v
 *     dv/dt = (-k*x + -d*v + u) / m
 *
 * where:
 *     m = mass (parameter)
 *     k = spring stiffness (parameter)
 *     d = damper coefficient (parameter)
 *     u = net applied force on the mass (input)
 *     x = position of the mass relative to equilibrium (state)
 *     v = velocity of the mass (state)
 *
 * This represents a second-order-system
 *
 * Implementation notes:
 * - Views the storage blocks through the structs from variables.h
 * - The state vector is a double array; States gives its entries names
 * - Writes result directly to the state derivatives
 */
void massSpringDamper(modelInstance* model)
{
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    const States* x  = (const States*)model->states;
    States*       dx = (States*)model->stateDerivatives;

    /* Compute state derivatives: dx/dt = v, dv/dt = (-k*x - d*v + u) / m */
    dx->x = x->v;
    dx->v = (-p->k * x->x - p->d * x->v + in->u) / p->m;
}

/**
 * Mass-Spring-Damper output equation: y = h(t, x, u)
 *
 * For this system, the outputs are simply equal to the states:
 *     y = x
 *
 * Implementation notes:
 * - Direct state passthrough (no transformation)
 * - Could be extended to include sensor dynamics or filtering
 */
void massSpringDamperOutputs(modelInstance* model)
{
    const States* x   = (const States*)model->states;
    Outputs*      out = model->outputs;

    /* Outputs equal states (y = x) */
    out->x = x->x;
    out->v = x->v;
}
