#include "unity.h"
#include "model.h"
#include "integrator.h"
#include "evaluate.h"
#include "simulation.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

/* Largest tableau of this test (fixed-size helper arrays, no VLA) */
#define MAX_STAGES 8

/* Tolerance of the order conditions and row sums: the coefficients are
   rounded fractions like 1/3 */
#define COEFFICIENT_TOLERANCE 1e-15

/***************************************************
 * Methods of this test, built as the docs show it
 *
 * Test fixtures, not built-in methods: each one is chosen for a path of the
 * engine it exercises (midpoint: a zero weight; Kutta 3: a negative
 * coefficient; 3/8 rule: a full lower triangle with negative entries).
 * A built-in method is added when a model needs it (ROADMAP D23).
 ***************************************************/

/* Explicit midpoint method, order 2: the example of docs/csfmu_gettingStarted.md */
static const double MIDPOINT_A[] = {
    0.0, 0.0,
    0.5, 0.0,
};
static const double MIDPOINT_B[] = { 0.0, 1.0 };
static const double MIDPOINT_C[] = { 0.0, 0.5 };

static const butcherTableau MIDPOINT_TABLEAU = {
    .stages = 2,
    .a      = MIDPOINT_A,
    .b      = MIDPOINT_B,
    .c      = MIDPOINT_C,
};

static simStatus midpointStep(modelInstance* model, double hMax, double* hTaken)
{
    *hTaken = hMax;
    return explicitRungeKuttaStep(model, &MIDPOINT_TABLEAU, hMax);
}

static const integratorDef MIDPOINT = {
    .name           = "explicit midpoint",
    .step           = midpointStep,
    .scratchVectors = EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(2),
    .order          = 2,
};

/* Kutta's method of order 3: a negative coefficient */
static const double KUTTA_3_A[] = {
     0.0, 0.0, 0.0,
     0.5, 0.0, 0.0,
    -1.0, 2.0, 0.0,
};
static const double KUTTA_3_B[] = { 1.0 / 6.0, 2.0 / 3.0, 1.0 / 6.0 };
static const double KUTTA_3_C[] = { 0.0, 0.5, 1.0 };

static const butcherTableau KUTTA_3_TABLEAU = {
    .stages = 3,
    .a      = KUTTA_3_A,
    .b      = KUTTA_3_B,
    .c      = KUTTA_3_C,
};

static simStatus kutta3Step(modelInstance* model, double hMax, double* hTaken)
{
    *hTaken = hMax;
    return explicitRungeKuttaStep(model, &KUTTA_3_TABLEAU, hMax);
}

static const integratorDef KUTTA_3 = {
    .name           = "Kutta 3",
    .step           = kutta3Step,
    .scratchVectors = EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(3),
    .order          = 3,
};

/* The 3/8 rule, order 4: every coefficient below the diagonal is used, some negative */
static const double THREE_EIGHTHS_A[] = {
     0.0,       0.0, 0.0, 0.0,
     1.0 / 3.0, 0.0, 0.0, 0.0,
    -1.0 / 3.0, 1.0, 0.0, 0.0,
     1.0,      -1.0, 1.0, 0.0,
};
static const double THREE_EIGHTHS_B[] = { 1.0 / 8.0, 3.0 / 8.0, 3.0 / 8.0, 1.0 / 8.0 };
static const double THREE_EIGHTHS_C[] = { 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0 };

static const butcherTableau THREE_EIGHTHS_TABLEAU = {
    .stages = 4,
    .a      = THREE_EIGHTHS_A,
    .b      = THREE_EIGHTHS_B,
    .c      = THREE_EIGHTHS_C,
};

static simStatus threeEighthsStep(modelInstance* model, double hMax, double* hTaken)
{
    *hTaken = hMax;
    return explicitRungeKuttaStep(model, &THREE_EIGHTHS_TABLEAU, hMax);
}

static const integratorDef THREE_EIGHTHS = {
    .name           = "3/8 rule",
    .step           = threeEighthsStep,
    .scratchVectors = EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(4),
    .order          = 4,
};

/* Tableaus the explicit engine must refuse */
static const double BACKWARD_EULER_ONE[] = { 1.0 };
static const butcherTableau BACKWARD_EULER_TABLEAU = {  /* a_11 = 1: implicit */
    .stages = 1, .a = BACKWARD_EULER_ONE, .b = BACKWARD_EULER_ONE, .c = BACKWARD_EULER_ONE,
};

static const double HALF_A[] = { 0.5 };  /* a_11 = 0.5 with c_1 = 0 */
static const double ZERO_C[] = { 0.0 };
static const double ONE_W[]  = { 1.0 };
static const butcherTableau DIAGONAL_TABLEAU = { .stages = 1, .a = HALF_A, .b = ONE_W, .c = ZERO_C };

static const double UPPER_A[] = { 0.0, 0.5, 0.0, 0.0 };  /* a_12 = 0.5 */
static const double UPPER_B[] = { 0.5, 0.5 };
static const double UPPER_C[] = { 0.0, 0.5 };
static const butcherTableau UPPER_TABLEAU = { .stages = 2, .a = UPPER_A, .b = UPPER_B, .c = UPPER_C };

static const double ZERO_A[] = { 0.0 };
static const double ONE_B[]  = { 1.0 };
static const double HALF_C[] = { 0.5 };
static const butcherTableau SHIFTED_NODE_TABLEAU = { .stages = 1, .a = ZERO_A, .b = ONE_B, .c = HALF_C };
static const butcherTableau NO_STAGES_TABLEAU    = { .stages = 0, .a = ZERO_A, .b = ONE_B, .c = ZERO_A };
static const butcherTableau NO_WEIGHTS_TABLEAU   = { .stages = 1, .a = ZERO_A, .b = NULL,  .c = ZERO_A };

/* A descriptor whose work memory is too small for its tableau */
static simStatus missizedStep(modelInstance* model, double hMax, double* hTaken)
{
    *hTaken = hMax;
    return explicitRungeKuttaStep(model, &RUNGE_KUTTA_4_TABLEAU, hMax);
}

static const integratorDef MISSIZED = {
    .name           = "Runge-Kutta 4 without work memory",
    .step           = missizedStep,
    .scratchVectors = 0,
    .order          = 4,
};

/***************************************************
 * Test model: dx/dt = -x + cos(t)
 ***************************************************/

/* x(t) for x(0) = x0. The -x term makes the true order visible: on a pure
   quadrature dx/dt = cos(t) Kutta 3 is Simpson's rule and shows order 4 */
static double exactSolution(double t, double x0)
{
    return 0.5 * (cos(t) + sin(t)) + (x0 - 0.5) * exp(-t);
}

static void forcedDecay(modelInstance* m)
{
    m->stateDerivatives[0] = -m->states[0] + cos(m->time);
}

static void noOutputs(modelInstance* m)
{
    (void)m;
}

/* Records the time of every derivative evaluation */
static double evaluationTimes[MAX_STAGES];
static int numberOfEvaluations;

static void recordTime(modelInstance* m)
{
    if (numberOfEvaluations < MAX_STAGES) {
        evaluationTimes[numberOfEvaluations] = m->time;
    }
    numberOfEvaluations++;
    m->stateDerivatives[0] = 0.0;
}

static const modelDefinition FORCED_DECAY = {
    .name                = "forced_decay",
    .numberOfStates      = 1,
    .getStateDerivatives = forcedDecay,
    .getOutputs          = noOutputs,
    .integrator          = &RUNGE_KUTTA_4,
    .maxStepSize         = 0.1,
};

static modelInstance* model;

void setUp(void)
{
    model = NULL;
}

void tearDown(void)
{
    freeModel(model);
}

/***************************************************
 * Helpers
 ***************************************************/

/* Coefficient a_ij, counted from 0 */
static double a(const butcherTableau* tableau, size_t i, size_t j)
{
    return tableau->a[i * tableau->stages + j];
}

static bool near(double value, double expected)
{
    return fabs(value - expected) <= COEFFICIENT_TOLERANCE;
}

/**
 * Highest order p <= 4 whose order conditions (one per rooted tree, Butcher)
 * the tableau meets:
 *   p = 1: sum b = 1
 *   p = 2: sum b c = 1/2
 *   p = 3: sum b c^2 = 1/3, sum b A c = 1/6
 *   p = 4: sum b c^3 = 1/4, sum b c A c = 1/8, sum b A c^2 = 1/12, sum b A A c = 1/24
 */
static int orderOf(const butcherTableau* tableau)
{
    size_t s = tableau->stages;
    TEST_ASSERT_TRUE(s <= MAX_STAGES);
    const double* b = tableau->b;
    const double* c = tableau->c;

    double Ac[MAX_STAGES] = { 0 };
    double Ac2[MAX_STAGES] = { 0 };
    for (size_t i = 0; i < s; i++) {
        for (size_t j = 0; j < s; j++) {
            Ac[i] += a(tableau, i, j) * c[j];
            Ac2[i] += a(tableau, i, j) * c[j] * c[j];
        }
    }
    double AAc[MAX_STAGES] = { 0 };
    for (size_t i = 0; i < s; i++) {
        for (size_t j = 0; j < s; j++) {
            AAc[i] += a(tableau, i, j) * Ac[j];
        }
    }

    double conditions[8] = { 0 };
    for (size_t i = 0; i < s; i++) {
        conditions[0] += b[i];
        conditions[1] += b[i] * c[i];
        conditions[2] += b[i] * c[i] * c[i];
        conditions[3] += b[i] * Ac[i];
        conditions[4] += b[i] * c[i] * c[i] * c[i];
        conditions[5] += b[i] * c[i] * Ac[i];
        conditions[6] += b[i] * Ac2[i];
        conditions[7] += b[i] * AAc[i];
    }

    if (!near(conditions[0], 1.0)) {
        return 0;
    }
    if (!near(conditions[1], 1.0 / 2.0)) {
        return 1;
    }
    if (!near(conditions[2], 1.0 / 3.0) || !near(conditions[3], 1.0 / 6.0)) {
        return 2;
    }
    if (!near(conditions[4], 1.0 / 4.0) || !near(conditions[5], 1.0 / 8.0) || !near(conditions[6], 1.0 / 12.0) ||
        !near(conditions[7], 1.0 / 24.0)) {
        return 3;
    }
    return 4;
}

/* The structure every explicit tableau has: c_1 = 0, zeros on and above the
   diagonal, and each node the sum of its row */
static void assertExplicitAndConsistent(const butcherTableau* tableau)
{
    size_t s = tableau->stages;
    TEST_ASSERT_EQUAL_DOUBLE(0.0, tableau->c[0]);
    for (size_t i = 0; i < s; i++) {
        double rowSum = 0.0;
        for (size_t j = 0; j < s; j++) {
            if (j >= i) {
                TEST_ASSERT_EQUAL_DOUBLE(0.0, a(tableau, i, j));
            }
            rowSum += a(tableau, i, j);
        }
        TEST_ASSERT_TRUE_MESSAGE(near(rowSum, tableau->c[i]), "c_i is not the sum of row i");
    }
}

/* Error at t = 2 of simulateTo() with the given method and step size */
static double errorAtTwo(const integratorDef* integrator, double stepSize)
{
    modelDefinition definition = FORCED_DECAY;
    definition.integrator = integrator;
    definition.maxStepSize = stepSize;

    modelInstance* m = newModel(&definition);
    TEST_ASSERT_NOT_NULL(m);
    m->states[0] = 1.0;
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(m, 2.0));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->time);
    double error = fabs(m->states[0] - exactSolution(2.0, 1.0));
    freeModel(m);
    return error;
}

/* Least-squares slope of log(error) over log(step size) */
static double convergenceOrder(const integratorDef* integrator)
{
    const double stepSizes[] = { 0.2, 0.1, 0.05 };
    size_t n = ARRAY_LEN(stepSizes);
    double meanX = 0.0, meanY = 0.0;
    double x[3], y[3];
    for (size_t k = 0; k < n; k++) {
        x[k] = log(stepSizes[k]);
        y[k] = log(errorAtTwo(integrator, stepSizes[k]));
        meanX += x[k] / (double)n;
        meanY += y[k] / (double)n;
    }
    double covariance = 0.0, variance = 0.0;
    for (size_t k = 0; k < n; k++) {
        covariance += (x[k] - meanX) * (y[k] - meanY);
        variance += (x[k] - meanX) * (x[k] - meanX);
    }
    return covariance / variance;
}

/* A refused step: SIM_ERROR, a message with the given text, nothing changed */
static void assertRefused(const butcherTableau* tableau, const char* reason)
{
    model->time = 0.25;
    model->states[0] = 1.5;
    model->stateDerivatives[0] = 2.0;

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, explicitRungeKuttaStep(model, tableau, 0.1));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(model->message, reason), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(0.25, model->time);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, model->states[0]);
}

/***************************************************
 * Tableaus
 ***************************************************/

void test_builtInTableaus_haveTheOrderOfTheirDescriptor(void)
{
    TEST_ASSERT_EQUAL_INT(FORWARD_EULER.order, orderOf(&FORWARD_EULER_TABLEAU));
    TEST_ASSERT_EQUAL_INT(RUNGE_KUTTA_4.order, orderOf(&RUNGE_KUTTA_4_TABLEAU));
}

void test_orderConditions_tellTheMethodsApart(void)
{
    /* forward Euler fails order 2, Kutta 3 fails order 4: the checker does
       not just pass everything */
    TEST_ASSERT_EQUAL_INT(1, orderOf(&FORWARD_EULER_TABLEAU));
    TEST_ASSERT_EQUAL_INT(2, orderOf(&MIDPOINT_TABLEAU));
    TEST_ASSERT_EQUAL_INT(3, orderOf(&KUTTA_3_TABLEAU));
    TEST_ASSERT_EQUAL_INT(4, orderOf(&THREE_EIGHTHS_TABLEAU));
}

void test_tableaus_areExplicitAndConsistent(void)
{
    assertExplicitAndConsistent(&FORWARD_EULER_TABLEAU);
    assertExplicitAndConsistent(&RUNGE_KUTTA_4_TABLEAU);
    assertExplicitAndConsistent(&MIDPOINT_TABLEAU);
    assertExplicitAndConsistent(&KUTTA_3_TABLEAU);
    assertExplicitAndConsistent(&THREE_EIGHTHS_TABLEAU);
}

void test_builtInDescriptors_haveTheWorkMemoryOfTheirTableau(void)
{
    TEST_ASSERT_EQUAL_size_t(EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(FORWARD_EULER_TABLEAU.stages),
                             FORWARD_EULER.scratchVectors);
    TEST_ASSERT_EQUAL_size_t(EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(RUNGE_KUTTA_4_TABLEAU.stages),
                             RUNGE_KUTTA_4.scratchVectors);
    TEST_ASSERT_EQUAL_size_t(0, EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(1));
    TEST_ASSERT_EQUAL_size_t(4, EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(4));
}

/***************************************************
 * The engine
 ***************************************************/

void test_builtInDescriptors_stepWithTheirTableau(void)
{
    /* the descriptor's step and the engine with its tableau: bit for bit */
    const integratorDef* descriptors[] = { &FORWARD_EULER, &RUNGE_KUTTA_4 };
    const butcherTableau* tableaus[] = { &FORWARD_EULER_TABLEAU, &RUNGE_KUTTA_4_TABLEAU };

    for (size_t k = 0; k < ARRAY_LEN(descriptors); k++) {
        modelDefinition definition = FORCED_DECAY;
        definition.integrator = descriptors[k];
        modelInstance* viaDescriptor = newModel(&definition);
        modelInstance* viaEngine = newModel(&definition);
        viaDescriptor->states[0] = viaEngine->states[0] = 1.0;

        for (int n = 0; n < 5; n++) {
            double taken = NAN;
            evaluateDerivatives(viaDescriptor);
            evaluateDerivatives(viaEngine);
            TEST_ASSERT_EQUAL_INT(SIM_OK, descriptors[k]->step(viaDescriptor, 0.1, &taken));
            TEST_ASSERT_EQUAL_DOUBLE(0.1, taken);
            TEST_ASSERT_EQUAL_INT(SIM_OK, explicitRungeKuttaStep(viaEngine, tableaus[k], 0.1));
        }
        TEST_ASSERT_EQUAL_MEMORY(viaEngine->states, viaDescriptor->states, sizeof(double));
        TEST_ASSERT_EQUAL_MEMORY(&viaEngine->time, &viaDescriptor->time, sizeof(double));
        freeModel(viaDescriptor);
        freeModel(viaEngine);
    }
}

void test_forwardEuler_isTheTextbookFormula(void)
{
    /* x + h * f, bit for bit */
    model = newModel(&FORCED_DECAY);
    model->time = 0.3;
    model->states[0] = 1.25;
    evaluateDerivatives(model);
    double expected = 1.25 + 0.1 * model->stateDerivatives[0];

    TEST_ASSERT_EQUAL_INT(SIM_OK, explicitRungeKuttaStep(model, &FORWARD_EULER_TABLEAU, 0.1));
    TEST_ASSERT_EQUAL_MEMORY(&expected, model->states, sizeof(double));
}

void test_rungeKutta4_isTheTextbookFormulaToRounding(void)
{
    /* k1 ... k4 written out; the engine sums h * (k1/6 + k2/3 + k3/3 + k4/6),
       so it may differ from (h/6) * (k1 + 2 k2 + 2 k3 + k4) by rounding only */
    double t = 0.3, x = 1.25, h = 0.1;
    double k1 = -x + cos(t);
    double k2 = -(x + 0.5 * h * k1) + cos(t + 0.5 * h);
    double k3 = -(x + 0.5 * h * k2) + cos(t + 0.5 * h);
    double k4 = -(x + h * k3) + cos(t + h);
    double expected = x + (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);

    model = newModel(&FORCED_DECAY);
    model->time = t;
    model->states[0] = x;
    evaluateDerivatives(model);
    TEST_ASSERT_EQUAL_INT(SIM_OK, explicitRungeKuttaStep(model, &RUNGE_KUTTA_4_TABLEAU, h));
    TEST_ASSERT_DOUBLE_WITHIN(4.0 * 2.220446049250313e-16 * fabs(expected), expected, model->states[0]);
}

void test_methods_convergeWithTheirOrder(void)
{
    /* slopes measured with a Python model of the same scheme: 2.07, 3.08, 4.07, 4.10 */
    const integratorDef* methods[] = { &MIDPOINT, &KUTTA_3, &THREE_EIGHTHS, &RUNGE_KUTTA_4 };
    for (size_t k = 0; k < ARRAY_LEN(methods); k++) {
        double order = convergenceOrder(methods[k]);
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.3, (double)methods[k]->order, order, methods[k]->name);
    }
}

void test_stages_areEvaluatedAtTheirNodes(void)
{
    modelDefinition definition = FORCED_DECAY;
    definition.integrator = &THREE_EIGHTHS;
    definition.getStateDerivatives = recordTime;
    model = newModel(&definition);
    model->time = 0.3;
    numberOfEvaluations = 0;

    /* k1 is evaluated by the caller at t = 0.3 */
    TEST_ASSERT_EQUAL_INT(SIM_OK, explicitRungeKuttaStep(model, &THREE_EIGHTHS_TABLEAU, 0.1));

    TEST_ASSERT_EQUAL_INT(3, numberOfEvaluations);
    TEST_ASSERT_EQUAL_DOUBLE(0.3 + (1.0 / 3.0) * 0.1, evaluationTimes[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.3 + (2.0 / 3.0) * 0.1, evaluationTimes[1]);
    TEST_ASSERT_EQUAL_DOUBLE(0.3 + 1.0 * 0.1, evaluationTimes[2]);
    TEST_ASSERT_EQUAL_DOUBLE(0.3 + 0.1, model->time);
}

void test_lastStage_staysInTheStateDerivatives(void)
{
    /* the work memory holds the states at the start and k1 ... k(s-1); the
       last stage is f at the last stage's state and time */
    modelDefinition definition = FORCED_DECAY;
    definition.integrator = &KUTTA_3;
    model = newModel(&definition);
    double t = 0.3, x = 1.25, h = 0.1;
    model->time = t;
    model->states[0] = x;
    evaluateDerivatives(model);
    double k1 = model->stateDerivatives[0];

    TEST_ASSERT_EQUAL_INT(SIM_OK, explicitRungeKuttaStep(model, &KUTTA_3_TABLEAU, h));

    double k2 = -(x + h * (0.5 * k1)) + cos(t + 0.5 * h);
    double k3 = -(x + h * (-1.0 * k1 + 2.0 * k2)) + cos(t + 1.0 * h);
    TEST_ASSERT_EQUAL_DOUBLE(x, model->scratch[0]);
    TEST_ASSERT_EQUAL_DOUBLE(k1, model->scratch[1]);
    TEST_ASSERT_EQUAL_DOUBLE(k2, model->scratch[2]);
    TEST_ASSERT_EQUAL_DOUBLE(k3, model->stateDerivatives[0]);
}

void test_modelWithoutStates_onlyAdvancesTheTime(void)
{
    static const modelDefinition NO_STATES = { .numberOfStates = 0, .integrator = &FORWARD_EULER };
    model = newModel(&NO_STATES);
    model->time = 1.0;

    /* no work memory needed, even for four stages */
    TEST_ASSERT_EQUAL_INT(SIM_OK, explicitRungeKuttaStep(model, &RUNGE_KUTTA_4_TABLEAU, 0.25));
    TEST_ASSERT_EQUAL_DOUBLE(1.25, model->time);
}

/***************************************************
 * Refused tableaus
 ***************************************************/

void test_engine_refusesAMissingOrIncompleteTableau(void)
{
    model = newModel(&FORCED_DECAY);
    assertRefused(NULL, "missing or incomplete");
    assertRefused(&NO_STAGES_TABLEAU, "missing or incomplete");
    assertRefused(&NO_WEIGHTS_TABLEAU, "missing or incomplete");
}

void test_engine_refusesAnImplicitTableau(void)
{
    model = newModel(&FORCED_DECAY);
    assertRefused(&BACKWARD_EULER_TABLEAU, "not explicit");  /* c_1 = 1 and a_11 = 1 */
    assertRefused(&DIAGONAL_TABLEAU, "row 1, column 1");
    assertRefused(&UPPER_TABLEAU, "row 1, column 2");
}

void test_engine_refusesAFirstStageAwayFromTheStart(void)
{
    model = newModel(&FORCED_DECAY);
    assertRefused(&SHIFTED_NODE_TABLEAU, "c_1");
}

void test_engine_refusesATableauThatNeedsMoreWorkMemory(void)
{
    /* allocated for forward Euler (no work memory), asked for four stages */
    modelDefinition definition = FORCED_DECAY;
    definition.integrator = &FORWARD_EULER;
    model = newModel(&definition);
    TEST_ASSERT_NULL(model->scratch);
    assertRefused(&RUNGE_KUTTA_4_TABLEAU, "work memory");
}

void test_simulateTo_reportsTheRefusalOfTheEngine(void)
{
    /* the engine's reason reaches the caller, and the error keeps the values */
    modelDefinition definition = FORCED_DECAY;
    definition.integrator = &MISSIZED;
    model = newModel(&definition);
    model->states[0] = 1.5;

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, 0.5));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(model->message, "EXPLICIT_RUNGE_KUTTA_SCRATCH_VECTORS(4)"), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(1.5, model->states[0]);
}

// ============ Test Runner ============

int main(void)
{
    UNITY_BEGIN();

    // Tableaus
    RUN_TEST(test_builtInTableaus_haveTheOrderOfTheirDescriptor);
    RUN_TEST(test_orderConditions_tellTheMethodsApart);
    RUN_TEST(test_tableaus_areExplicitAndConsistent);
    RUN_TEST(test_builtInDescriptors_haveTheWorkMemoryOfTheirTableau);

    // The engine
    RUN_TEST(test_builtInDescriptors_stepWithTheirTableau);
    RUN_TEST(test_forwardEuler_isTheTextbookFormula);
    RUN_TEST(test_rungeKutta4_isTheTextbookFormulaToRounding);
    RUN_TEST(test_methods_convergeWithTheirOrder);
    RUN_TEST(test_stages_areEvaluatedAtTheirNodes);
    RUN_TEST(test_lastStage_staysInTheStateDerivatives);
    RUN_TEST(test_modelWithoutStates_onlyAdvancesTheTime);

    // Refused tableaus
    RUN_TEST(test_engine_refusesAMissingOrIncompleteTableau);
    RUN_TEST(test_engine_refusesAnImplicitTableau);
    RUN_TEST(test_engine_refusesAFirstStageAwayFromTheStart);
    RUN_TEST(test_engine_refusesATableauThatNeedsMoreWorkMemory);
    RUN_TEST(test_simulateTo_reportsTheRefusalOfTheEngine);

    return UNITY_END();
}
