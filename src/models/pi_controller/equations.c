#include "variables.h"

/**
 * PI controller state equation: dx/dt = f(t, x, u)
 *
 * Implements the differential equation:
 *     dx_I/dt = e = r - y_m
 *
 * where:
 *     r   = reference (input)
 *     y_m = measured plant output (input)
 *     x_I = integral of the control error (state)
 *
 * Implementation notes:
 * - The control error is computed here and again in the output equation:
 *   the outputs are not up to date while the integrator evaluates its stages
 */
void piControllerSystem(modelInstance* model)
{
    const Inputs* in = model->inputs;
    States*       dx = (States*)model->stateDerivatives;

    dx->x_I = in->r - in->y_m;
}

/**
 * PI controller output equation: y = h(t, x, u)
 *
 * The outputs are:
 *     e = r - y_m
 *     u = Kp*e + Ki*x_I
 *
 * Implementation notes:
 * - Both outputs use the inputs directly (direct feedthrough), hence
 *   .dependsOn = "r y_m" in variables.c. Setting r or y_m changes u and e
 *   immediately, without a simulation step
 */
void piControllerOutputs(modelInstance* model)
{
    const Params* p   = model->params;
    const Inputs* in  = model->inputs;
    const States* x   = (const States*)model->states;
    Outputs*      out = model->outputs;

    out->e = in->r - in->y_m;
    out->u = p->Kp * out->e + p->Ki * x->x_I;
}
