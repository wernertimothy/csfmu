#include "variables.h"

/**
 * Heat flow from the mass to the ambient: Q_loss = (T - T_amb)/R
 *
 * Shared by the state equation and the output equation. The state equation
 * needs the heat flow at every stage of the integrator, while the local
 * variable Q_loss is only computed at the communication points, so the
 * state equation computes it itself instead of reading the local variable.
 */
static double heatLoss(const Params* p, const States* x)
{
    return (x->T - p->T_amb) / p->R;
}

/**
 * Thermal mass state equation: dT/dt = f(t, T, P)
 *
 * Implements the energy balance:
 *     C*dT/dt = P - Q_loss,   Q_loss = (T - T_amb)/R
 *
 * where:
 *     C     = heat capacity (parameter)
 *     R     = thermal resistance to the ambient (parameter)
 *     T_amb = ambient temperature (parameter)
 *     P     = heating power (input)
 *     T     = temperature of the mass (state)
 */
void thermalMassSystem(modelInstance* model)
{
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    const States* x  = (const States*)model->states;
    States*       dx = (States*)model->stateDerivatives;

    dx->T = (in->P - heatLoss(p, x)) / p->C;
}

/**
 * Thermal mass output equation: outputs and local variables
 *
 *     T_out  = T
 *     Q_loss = (T - T_amb)/R   (local variable)
 *
 * Implementation notes:
 * - No direct feedthrough of P, hence .dependsOn = "" in variables.c
 * - Local variables are computed here, together with the outputs
 */
void thermalMassOutputs(modelInstance* model)
{
    const Params* p      = model->params;
    const States* x      = (const States*)model->states;
    Outputs*      out    = model->outputs;
    Locals*       locals = model->locals;

    out->T_out     = x->T;
    locals->Q_loss = heatLoss(p, x);
}
