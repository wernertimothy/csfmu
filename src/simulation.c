#include <float.h>
#include <math.h>
#include "simulation.h"
#include "evaluate.h"

/* Tolerance on the number of integration steps: absorbs rounding in
   (tEnd - t) / maxStepSize, so that 0.3 / 0.1 gives 3 steps and not 4 */
#define SUBSTEP_TOLERANCE 1e-9

/* Largest number of integration steps of one call */
#define MAX_NUMBER_OF_SUBSTEPS 1e9

/* Allowed deviation of model->time from t + h after an integration step,
   as a fraction of the step size h (beyond rounding, see timeRoundingTolerance()) */
#define INTEGRATOR_TIME_FRACTION 1e-6

double timeRoundingTolerance(double time)
{
    return 8.0 * DBL_EPSILON * fmax(1.0, fabs(time));
}

/**
 * Ends a simulation that failed with the given status: a numerical failure
 * (SIM_DISCARD) restores the model data of the start, an error keeps the
 * values for diagnosis
 */
static simStatus endFailed(modelInstance* model, simStatus status)
{
    if (status == SIM_DISCARD) {
        restoreModelData(model, model->stepStart);
    }
    return status;
}

/**
 * Implementation notes for simulateTo():
 * - All checks that can refuse the call come before saveModelData(), so a
 *   refused call changes nothing
 * - The step size is (tEnd - t) / N, so the integrated length equals the
 *   advance of the clock exactly; the start of step k is t + k * h from the
 *   integer k, and the end is tEnd itself, so rounding does not add up
 * - The integrator contract is a programming error when broken (SIM_ERROR,
 *   no rollback), a non-finite state or result a numerical one (SIM_DISCARD,
 *   rollback to the start, not to the last good step)
 */
simStatus simulateTo(modelInstance* model, double tEnd)
{
    const modelDefinition* def = model->def;
    double start = model->time;
    model->message[0] = '\0';

    if (!(isfinite(tEnd) && tEnd >= start)) {
        return modelError(model, "Cannot simulate from t = %.16g to t = %.16g: the end must be finite and not "
                                 "before the start.", start, tEnd);
    }
    if (!(def->maxStepSize > 0.0)) {
        return modelError(model, "maxStepSize must be greater than 0, not %.16g.", def->maxStepSize);
    }

    double length = tEnd - start;
    double numberOfSteps = 0.0;
    if (length > 0.0) {
        numberOfSteps = fmax(1.0, ceil(length / def->maxStepSize - SUBSTEP_TOLERANCE));
        if (numberOfSteps > MAX_NUMBER_OF_SUBSTEPS) {
            return modelError(model, "The step from t = %.16g to t = %.16g needs more than %.0f integration steps "
                                     "of at most maxStepSize = %.16g.", start, tEnd, MAX_NUMBER_OF_SUBSTEPS,
                              def->maxStepSize);
        }
    }
    double stepSize = (numberOfSteps > 0.0) ? length / numberOfSteps : 0.0;

    /* Remember the start to roll back a failed step */
    saveModelData(model, model->stepStart);

    for (size_t k = 0; k < (size_t)numberOfSteps; k++) {
        double subStepStart = start + (double)k * stepSize;
        model->time = subStepStart;

        simStatus status = evaluateDerivatives(model);
        if (status != SIM_OK) {
            return endFailed(model, status);
        }
        model->integrate(model, stepSize);

        /* The integrator contract: on return model->time is t + h */
        double expectedTime = subStepStart + stepSize;
        if (!(fabs(model->time - expectedTime) <=
              fmax(INTEGRATOR_TIME_FRACTION * stepSize, timeRoundingTolerance(expectedTime)))) {
            return modelError(model, "The integrator left model->time at %.16g instead of %.16g (t + h). "
                                     "See the integrator contract in include/model.h.", model->time, expectedTime);
        }

        size_t failed = modelFirstNonFiniteState(model);
        if (failed < def->numberOfStates) {
            double failureTime = model->time;
            restoreModelData(model, model->stepStart);
            return modelDiscard(model, "State '%s' is not finite at t = %.16g.", modelStateName(def, failed),
                                failureTime);
        }
    }

    /* The end, exactly; the outputs and locals are evaluated there */
    model->time = tEnd;
    model->getOutputs(model);

    size_t failedResult = modelFirstNonFiniteResult(model);
    if (failedResult < def->numberOfVariables) {
        const modelVariable* failed = &def->variables[failedResult];
        restoreModelData(model, model->stepStart);
        return modelDiscard(model, "%s '%s' is not finite at t = %.16g.", modelResultKind(failed), failed->name,
                            tEnd);
    }

    return SIM_OK;
}
