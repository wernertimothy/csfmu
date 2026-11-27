#include <string.h>
#include "integrator.h"
#include "evaluate.h"

/* Coefficient a_ij of a tableau, i and j counted from 0 */
static double coefficient(const butcherTableau* tableau, size_t i, size_t j)
{
    return tableau->a[i * tableau->stages + j];
}

/**
 * Stage k_j (counted from 0) of a step with the given number of stages: the
 * stages before the last are in the work memory behind the states at the
 * start, the last one is in model->stateDerivatives
 */
static const double* stage(const modelInstance* model, size_t stages, size_t j)
{
    size_t nx = model->def->numberOfStates;
    return (j + 1 < stages) ? model->scratch + (j + 1) * nx : model->stateDerivatives;
}

/**
 * Implementation notes for combine():
 * - states = start + h * sum_j weights[j] * k_j over the first count stages;
 *   a row of the tableau gives a stage, the weights b the solution
 * - Stages and solution use this one function, so a method whose last row
 *   equals its weights (first same as last) gets a solution that is bit for
 *   bit the state of its last stage
 * - Terms with a weight of exactly 0 are left out: they add nothing, and a
 *   non-finite stage must not turn 0 * k into NaN where it is not used
 * - Works per component, so start may be model->states itself
 */
static void combine(modelInstance* model, const double* start, size_t stages, const double* weights, size_t count,
                    double h)
{
    size_t nx = model->def->numberOfStates;
    for (size_t i = 0; i < nx; i++) {
        double sum = 0.0;
        for (size_t j = 0; j < count; j++) {
            if (weights[j] != 0.0) {
                sum += weights[j] * stage(model, stages, j)[i];
            }
        }
        model->states[i] = start[i] + h * sum;
    }
}

/**
 * Checks that a tableau describes an explicit method and that the instance
 * has the work memory for it. The work memory is the one newModel()
 * allocated for the definition's integrator
 */
static simStatus checkTableau(modelInstance* model, const butcherTableau* tableau)
{
    if (!tableau || tableau->stages == 0 || !tableau->a || !tableau->b || !tableau->c) {
        return modelError(model, "The Butcher tableau is missing or incomplete: it needs at least one stage and "
                                 "the coefficients a, b and c.");
    }

    size_t stages = tableau->stages;
    if (tableau->c[0] != 0.0) {
        return modelError(model, "The Butcher tableau is not explicit: c_1 is %.16g, but the first stage of an "
                                 "explicit method is f(t, x), so c_1 must be 0.", tableau->c[0]);
    }
    for (size_t i = 0; i < stages; i++) {
        for (size_t j = i; j < stages; j++) {
            if (coefficient(tableau, i, j) != 0.0) {
                return modelError(model, "The Butcher tableau is not explicit: a_ij in row %zu, column %zu "
                                         "(counted from 1) is %.16g, but must be 0 on and above the diagonal.",
                                  i + 1, j + 1, coefficient(tableau, i, j));
            }
        }
    }

    const integratorDef* integrator = model->def->integrator;
    size_t available = integrator ? integrator->scratchVectors : 0;
    size_t needed = EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(stages);
    if (model->def->numberOfStates > 0 && needed > available) {
        return modelError(model, "A Butcher tableau of %zu stages needs %zu vectors of work memory, but the "
                                 "instance has %zu: set .scratchVectors = EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(%zu) "
                                 "in the descriptor of the model's integrator.", stages, needed, available, stages);
    }
    return SIM_OK;
}

/**
 * Implementation notes for explicitRungeKuttaStep():
 * - All checks come first, so a refused step changes nothing
 * - k_1 = f(t, x) is the derivative simulateTo() evaluated at the start; the
 *   further stages are evaluated with evaluateDerivatives() at their own
 *   times t + c_i * h, so that time-varying models keep the order
 * - Work memory: the states at the start and k_1 ... k_(s-1) in
 *   model->scratch; the last stage stays in model->stateDerivatives, so it is
 *   never copied. A single stage combines in place and needs none
 * - A model without states has no stages to evaluate and only advances the
 *   time
 * - A failed stage evaluation returns its status at once; the states then
 *   hold an intermediate value, and simulateTo() restores or keeps them
 */
simStatus explicitRungeKuttaStep(modelInstance* model, const butcherTableau* tableau, double h)
{
    simStatus status = checkTableau(model, tableau);
    if (status != SIM_OK) {
        return status;
    }

    double t0 = model->time;
    size_t nx = model->def->numberOfStates;
    size_t stages = tableau->stages;

    if (nx > 0 && stages == 1) {
        /* x(t + h) = x(t) + h * b_1 * k_1, with k_1 = f(t, x) */
        combine(model, model->states, stages, tableau->b, stages, h);
    } else if (nx > 0) {
        double* start = model->scratch;
        memcpy(start, model->states, nx * sizeof(double));
        memcpy(model->scratch + nx, model->stateDerivatives, nx * sizeof(double));

        /* k_i = f(t + c_i * h, x + h * sum_j a_ij * k_j), with the stages before it */
        for (size_t i = 1; i < stages; i++) {
            combine(model, start, stages, &tableau->a[i * stages], i, h);
            model->time = t0 + tableau->c[i] * h;
            if ((status = evaluateDerivatives(model)) != SIM_OK) {
                return status;
            }
            if (i + 1 < stages) {
                memcpy(model->scratch + (i + 1) * nx, model->stateDerivatives, nx * sizeof(double));
            }
        }

        /* x(t + h) = x(t) + h * sum_j b_j * k_j */
        combine(model, start, stages, tableau->b, stages, h);
    }

    model->time = t0 + h;
    return SIM_OK;
}

/***************************************************
 * Forward Euler
 ***************************************************/

static const double FORWARD_EULER_A[] = { 0.0 };
static const double FORWARD_EULER_B[] = { 1.0 };
static const double FORWARD_EULER_C[] = { 0.0 };

const butcherTableau FORWARD_EULER_TABLEAU = {
    .stages = ARRAY_LEN(FORWARD_EULER_B),
    .a      = FORWARD_EULER_A,
    .b      = FORWARD_EULER_B,
    .c      = FORWARD_EULER_C,
};

static simStatus forwardEulerStep(modelInstance* model, double hMax, double* hTaken)
{
    *hTaken = hMax;
    return explicitRungeKuttaStep(model, &FORWARD_EULER_TABLEAU, hMax);
}

const integratorDef FORWARD_EULER = {
    .name           = "Forward Euler",
    .step           = forwardEulerStep,
    .scratchVectors = EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(ARRAY_LEN(FORWARD_EULER_B)),
    .order          = 1,
};

/***************************************************
 * Classic Runge-Kutta method of order 4
 ***************************************************/

static const double RUNGE_KUTTA_4_A[] = {
    0.0, 0.0, 0.0, 0.0,
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
};
static const double RUNGE_KUTTA_4_B[] = { 1.0 / 6.0, 1.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
static const double RUNGE_KUTTA_4_C[] = { 0.0, 0.5, 0.5, 1.0 };

const butcherTableau RUNGE_KUTTA_4_TABLEAU = {
    .stages = ARRAY_LEN(RUNGE_KUTTA_4_B),
    .a      = RUNGE_KUTTA_4_A,
    .b      = RUNGE_KUTTA_4_B,
    .c      = RUNGE_KUTTA_4_C,
};

static simStatus rungeKutta4Step(modelInstance* model, double hMax, double* hTaken)
{
    *hTaken = hMax;
    return explicitRungeKuttaStep(model, &RUNGE_KUTTA_4_TABLEAU, hMax);
}

const integratorDef RUNGE_KUTTA_4 = {
    .name           = "Runge-Kutta 4",
    .step           = rungeKutta4Step,
    .scratchVectors = EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(ARRAY_LEN(RUNGE_KUTTA_4_B)),
    .order          = 4,
};
