#include "variables.h"

/**
 * First-order lag system (PT1) state equation: dx/dt = f(t, x, u)
 *
 * Implements the differential equation:
 *     dx/dt = (-x + k*u) / tau
 *
 * where:
 *     tau = time constant (parameter)
 *     k   = system gain (parameter)
 *     u   = input signal (input)
 *     x   = state variable (state)
 *
 * This represents a first-order low-pass filter or lag element.
 *
 * Implementation notes:
 * - Views the storage blocks through the structs from variables.h
 * - The state vector is a double array; States gives its entries names
 * - Writes result directly to the state derivatives
 */
void firstOrderSystem(modelInstance* model)
{
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    const States* x  = (const States*)model->states;
    States*       dx = (States*)model->stateDerivatives;

    /* Compute state derivative: dx/dt = (-x + k*u) / tau */
    dx->x = (-x->x + p->k * in->u) / p->tau;
}

/**
 * First-order system output equation: y = h(t, x, u)
 *
 * For this system, the output is simply equal to the state:
 *     y = x
 *
 * Implementation notes:
 * - Direct state passthrough (no transformation)
 * - Could be extended to include sensor dynamics or filtering
 */
void firstOrderOutputs(modelInstance* model)
{
    const States* x   = (const States*)model->states;
    Outputs*      out = model->outputs;

    /* Output equals state (y = x) */
    out->y = x->x;
}
