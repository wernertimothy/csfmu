#include <math.h>
#include "variables.h"

/**
 * Fault injection state equation: dx/dt = f(t, x)
 *
 * Implements the differential equation:
 *     dx/dt = -x / tau
 *
 * and injects a numerical failure: if fail is set, the derivative is NaN
 * for t >= t_fail. The integration then produces a NaN state and fmi2DoStep
 * discards the step (fmi2Discard) and rolls the FMU back.
 *
 * where:
 *     tau    = time constant (parameter)
 *     t_fail = time from which on the fault is active (parameter, tunable)
 *     fail   = switches the fault on (parameter, tunable)
 *     x      = state variable (state)
 *     t      = time (model->time, the time of the integrator stage)
 *
 * Implementation notes:
 * - Returning NaN is how a model signals "cannot be evaluated here"
 */
void faultInjectionSystem(modelInstance* model)
{
    const Params* p  = model->params;
    const States* x  = (const States*)model->states;
    States*       dx = (States*)model->stateDerivatives;

    if (p->fail && model->time >= p->t_fail) {
        dx->x = NAN;
    } else {
        dx->x = -x->x / p->tau;
    }
}

/**
 * Fault injection output equation: y = h(x)
 *
 * The output is:
 *     y = x
 *
 * Implementation notes:
 * - The model has no inputs, hence no feedthrough (.dependsOn = "")
 */
void faultInjectionOutputs(modelInstance* model)
{
    const States* x   = (const States*)model->states;
    Outputs*      out = model->outputs;

    out->y = x->x;
}
