#include "evaluate.h"

/**
 * Implementation notes for evaluateDerivatives():
 * - Today it only calls the state equation. Later steps plug in here, in
 *   this order: the input provider (inputs at the current time), the
 *   solution of the algebraic equations, the state equation, the solution
 *   with the mass matrix
 * - Calls model->getStateDerivatives (not def->getStateDerivatives), so
 *   that a caller may replace the state equation of an instance
 */
simStatus evaluateDerivatives(modelInstance* model)
{
    model->getStateDerivatives(model);
    return SIM_OK;
}
