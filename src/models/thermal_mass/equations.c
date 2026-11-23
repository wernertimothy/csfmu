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

/**
 * Calculated parameters: tau = R*C
 *
 * Reads nothing but the parameters: it is called whenever a parameter may
 * have changed, also before the initialization.
 */
void thermalMassParameters(modelInstance* model)
{
    Params* p = model->params;

    p->tau = p->R * p->C;
}

/**
 * Initialization, called once when the initialization is finished
 *
 *     fixed_start:  T keeps its start value
 *     steady_state: T = T_amb + R*P, so that dT/dt = 0 for the initial P
 *
 * Implementation notes:
 * - T is declared with .initial = VAR_APPROX in variables.c, because this
 *   function may replace its start value
 * - init_mode is checked here: the bounds in the variable table are
 *   published, but the FMU does not enforce them
 */
simStatus thermalMassInitialize(modelInstance* model)
{
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    States*       x  = (States*)model->states;

    switch (p->init_mode) {
        case THERMAL_MASS_FIXED_START:
            return SIM_OK;
        case THERMAL_MASS_STEADY_STATE:
            x->T = p->T_amb + p->R * in->P;
            return SIM_OK;
        default:
            return modelError(model, "init_mode %d is neither %d (fixed_start) nor %d (steady_state).",
                              p->init_mode, THERMAL_MASS_FIXED_START, THERMAL_MASS_STEADY_STATE);
    }
}
