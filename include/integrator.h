/**
 * @file integrator.h
 * @brief The built-in integration methods, the contract of a step and the
 *        explicit Runge-Kutta methods given by their Butcher tableau.
 *
 * A model selects its method in its definition:
 *
 *     .integrator  = &RUNGE_KUTTA_4,
 *     .maxStepSize = 1e-3,
 *
 * simulateTo() (simulation.h) cuts every interval into equal steps of at most
 * maxStepSize and calls the method's step() once per step.
 *
 * Contract of step(model, hMax, &hTaken), for the built-in methods and for a
 * descriptor a model brings itself (see docs/csfmu_gettingStarted.md):
 *
 * - On entry model->time is the start t of the step and
 *   model->stateDerivatives holds f(t, x, u): simulateTo() has evaluated it.
 *   model->scratch holds scratchVectors * nx doubles of undefined content.
 * - For every further stage the method sets model->time to the time of the
 *   stage and calls evaluateDerivatives() (evaluate.h), never the model's
 *   state equation directly. If that does not return SIM_OK, step() returns
 *   the same status at once; simulateTo() then restores the model (SIM_DISCARD)
 *   or reports the error (SIM_ERROR).
 * - On return the states and model->time are at t + *hTaken. A fixed-step
 *   method takes the whole step: *hTaken = hMax. simulateTo() checks both, up
 *   to rounding, and fails with SIM_ERROR otherwise. The content of
 *   model->stateDerivatives is undefined: simulateTo() evaluates f again at
 *   the start of the next step.
 *
 * Explicit Runge-Kutta methods
 *
 * An s-stage Runge-Kutta method is given by its Butcher tableau
 *
 *     c_1 | a_11 ... a_1s
 *      .  |  .        .
 *     c_s | a_s1 ... a_ss
 *     ----+--------------
 *         | b_1  ... b_s
 *
 * and takes the step
 *
 *     k_i      = f(t + c_i*h, x + h * sum_j a_ij * k_j)     i = 1 ... s
 *     x(t + h) = x(t) + h * sum_j b_j * k_j
 *
 * The method is explicit if a_ij = 0 for j >= i: every stage only uses the
 * stages before it. Its first stage is then f(t, x), so c_1 = 0.
 * explicitRungeKuttaStep() takes such a step for any explicit tableau, and
 * FORWARD_EULER and RUNGE_KUTTA_4 are built on it. A model brings its own
 * method the same way: a tableau, a step function that passes it to
 * explicitRungeKuttaStep(), and a descriptor with
 * EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(s) vectors of work memory (see
 * docs/csfmu_gettingStarted.md).
 *
 * FMI-agnostic: the model layer does not include an FMI header.
 */

#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Butcher tableau of an s-stage Runge-Kutta method.
 *
 * The coefficient matrix is stored in full, row by row (a[i*s + j] = a_ij,
 * counted from 0), so that the same type also describes implicit methods.
 */
typedef struct butcherTableau {
    size_t stages;    /**< Number of stages s, at least 1 */
    const double* a;  /**< Coefficients a_ij [s*s], row-major */
    const double* b;  /**< Weights b_j [s] */
    const double* c;  /**< Nodes c_i [s]: stage i is evaluated at t + c_i*h */
} butcherTableau;

/**
 * @brief Work memory explicitRungeKuttaStep() needs for a tableau of s stages.
 *
 * In vectors of length nx, for the .scratchVectors of a descriptor: the
 * states at the start and the stages k_1 ... k_(s-1). The last stage stays
 * in model->stateDerivatives. A single stage (forward Euler) needs none.
 * A constant expression, so it can initialize a static descriptor.
 */
#define EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(stages) ((stages) > 1 ? (size_t)(stages) : (size_t)0)

/**
 * @brief Takes one step of an explicit Runge-Kutta method.
 *
 * Advances the states from model->time by exactly h with the given tableau,
 * under the contract of step() above: on entry model->stateDerivatives holds
 * f(t, x, u), which is the first stage. Every further stage is evaluated with
 * evaluateDerivatives() at its own time t + c_i*h. On return the states and
 * model->time are at t + h. Terms with a coefficient of exactly 0 are left
 * out of the sums.
 *
 * A step function of a descriptor calls it with its tableau:
 *
 *     static simStatus myStep(modelInstance* model, double hMax, double* hTaken)
 *     {
 *         *hTaken = hMax;
 *         return explicitRungeKuttaStep(model, &MY_TABLEAU, hMax);
 *     }
 *
 * @param model   Pointer to the model instance. Its work memory
 *                (model->scratch) must hold
 *                EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(tableau->stages)
 *                vectors: the .scratchVectors of the definition's integrator
 * @param tableau The Butcher tableau of an explicit method
 * @param h       Step size
 *
 * @return
 * - SIM_OK: the states and model->time are at t + h.
 * - The status of a stage evaluation that did not return SIM_OK, at once.
 *   The states then hold an intermediate value; simulateTo() restores or
 *   keeps them.
 * - SIM_ERROR with the reason in model->message if the tableau is missing or
 *   incomplete, is not explicit (c_1 != 0, or a_ij != 0 for j >= i), or needs
 *   more work memory than the instance has. The states and model->time are
 *   then unchanged.
 */
simStatus explicitRungeKuttaStep(modelInstance* model, const butcherTableau* tableau, double h);

/**
 * @brief Butcher tableau of the forward Euler method.
 *
 *     0 | 0
 *     --+--
 *       | 1
 */
extern const butcherTableau FORWARD_EULER_TABLEAU;

/**
 * @brief Butcher tableau of the classic Runge-Kutta method of order 4.
 *
 *     0   |
 *     1/2 | 1/2
 *     1/2 | 0    1/2
 *     1   | 0    0    1
 *     ----+-------------------
 *         | 1/6  1/3  1/3  1/6
 */
extern const butcherTableau RUNGE_KUTTA_4_TABLEAU;

/**
 * @brief Forward (explicit) Euler method, order 1.
 *
 *     x(t + h) = x(t) + h * f(t, x)
 *
 * Uses the derivatives simulateTo() evaluated at the start, so a step costs
 * no further evaluation. Conditionally stable: maxStepSize must be small
 * enough for the fastest dynamics of the model. Needs no work memory.
 * Tableau: FORWARD_EULER_TABLEAU.
 */
extern const integratorDef FORWARD_EULER;

/**
 * @brief Classic Runge-Kutta method, order 4.
 *
 *     k1 = f(t,       x)
 *     k2 = f(t + h/2, x + h/2 * k1)
 *     k3 = f(t + h/2, x + h/2 * k2)
 *     k4 = f(t + h,   x + h * k3)
 *     x(t + h) = x(t) + h * (k1/6 + k2/3 + k3/3 + k4/6)
 *
 * k1 is the derivative simulateTo() evaluated at the start, so a step costs
 * three further evaluations, each at its own stage time. Suits most non-stiff
 * systems. Needs 4 vectors of work memory. Tableau: RUNGE_KUTTA_4_TABLEAU.
 */
extern const integratorDef RUNGE_KUTTA_4;

#ifdef __cplusplus
}
#endif

#endif
