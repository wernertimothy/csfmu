#include "unity.h"
#include "model.h"
#include "integrator.h"
#include <math.h>

#define TOLERANCE 1e-10

/* Minimal model definitions: the integrators only need the number of states */
static const modelDefinition ONE_STATE    = { .numberOfStates = 1, .integrator = &RUNGE_KUTTA_4 };
static const modelDefinition TWO_STATES   = { .numberOfStates = 2, .integrator = &RUNGE_KUTTA_4 };
static const modelDefinition NO_STATES    = { .numberOfStates = 0, .integrator = &RUNGE_KUTTA_4 };

/* Model with one state and one parameter (decay rate k) */
typedef struct { double k; } Params;
static const modelDefinition ONE_STATE_ONE_PARAM = { .numberOfStates = 1, .sizeOfParams = sizeof(Params),
                                                       .integrator = &RUNGE_KUTTA_4 };

static modelInstance* model;

/* One step of RUNGE_KUTTA_4: it succeeds and takes the whole step (fixed-step contract) */
static void takeStep(modelInstance* m, double h)
{
    double taken = NAN;
    TEST_ASSERT_EQUAL_INT(SIM_OK, RUNGE_KUTTA_4.step(m, h, &taken));
    TEST_ASSERT_EQUAL_DOUBLE(h, taken);
}

/* Always returns the pre-set stateDerivatives unchanged */
static void constantDerivative(modelInstance* m) {
    (void)m;
}

/* Simple derivative: dx/dt = -k * x (exponential decay) */
static void exponentialDecay(modelInstance* m) {
    double k = ((const Params*)m->params)->k;
    m->stateDerivatives[0] = -k * m->states[0];
}

/* Two-state system: dx1/dt = x2, dx2/dt = -x1 (harmonic oscillator) */
static void harmonicOscillator(modelInstance* m) {
    m->stateDerivatives[0] = m->states[1];
    m->stateDerivatives[1] = -m->states[0];
}

/* Time-dependent derivative: dx/dt = cos(t), exact solution x(t) = sin(t) */
static void cosineOfTime(modelInstance* m) {
    m->stateDerivatives[0] = cos(m->time);
}

/* Records the time of every derivative evaluation */
static double evaluationTimes[8];
static int numberOfEvaluations;

static void recordTime(modelInstance* m) {
    if (numberOfEvaluations < 8) {
        evaluationTimes[numberOfEvaluations] = m->time;
    }
    numberOfEvaluations++;
    m->stateDerivatives[0] = 0.0;
}

void setUp(void) {
    model = NULL;
}

void tearDown(void) {
    freeModel(model);
}

void test_rungeKutta4_singleStateWithConstantDerivative(void) {
    model = newModel(&ONE_STATE);
    model->stateDerivatives[0] = 2.0;
    model->getStateDerivatives = constantDerivative;

    /* With constant derivative, RK4 gives exact result: x = 0 + 2.0 * 0.1 = 0.2 */
    takeStep(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.2, model->states[0]);
}

void test_rungeKutta4_zeroStepSize(void) {
    model = newModel(&ONE_STATE);
    model->states[0] = 5.0;
    model->stateDerivatives[0] = 1.0;
    model->getStateDerivatives = constantDerivative;

    takeStep(model, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, model->states[0]);
}

void test_rungeKutta4_modelWithoutStates(void) {
    /* A model without states has no state storage; the step must do nothing */
    model = newModel(&NO_STATES);
    model->getStateDerivatives = constantDerivative;

    takeStep(model, 0.1);

    TEST_ASSERT_NULL(model->states);
    TEST_ASSERT_NULL(model->stateDerivatives);
}

void test_rungeKutta4_multipleStates(void) {
    model = newModel(&TWO_STATES);
    model->states[0] = 1.0;    /* x1(0) = 1 */
    model->states[1] = 0.0;    /* x2(0) = 0 */
    model->getStateDerivatives = harmonicOscillator;

    /* dx1/dt = x2, dx2/dt = -x1 → harmonic oscillator */
    /* Analytical: x1(t) = cos(t), x2(t) = -sin(t) */
    model->getStateDerivatives(model);

    double dt = 0.1;
    takeStep(model, dt);

    TEST_ASSERT_DOUBLE_WITHIN(1e-7, cos(0.1), model->states[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -sin(0.1), model->states[1]);
}

void test_rungeKutta4_exponentialDecay(void) {
    model = newModel(&ONE_STATE_ONE_PARAM);
    model->states[0] = 1.0;
    ((Params*)model->params)->k = 1.0;  /* k = 1.0 */
    model->getStateDerivatives = exponentialDecay;

    double dt = 0.1;
    double T = 1.0;
    int steps = (int)(T / dt);

    for (int i = 0; i < steps; i++) {
        model->getStateDerivatives(model);
        takeStep(model, dt);
    }

    /* Analytical: x(1) = exp(-1) ≈ 0.36788 */
    double analytical = exp(-1.0);

    /* RK4 with dt=0.1 has 4th-order error — use 1e-6 tolerance */
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, analytical, model->states[0]);
}

void test_rungeKutta4_orderOfAccuracy(void) {
    /* Run with step size h, then h/2, verify error ratio ≈ 16 (2^4) */
    double analytical = exp(-1.0);
    double errors[2];
    double stepSizes[2] = {0.2, 0.1};

    for (int run = 0; run < 2; run++) {
        modelInstance* m = newModel(&ONE_STATE_ONE_PARAM);
        m->states[0] = 1.0;
        ((Params*)m->params)->k = 1.0;
        m->getStateDerivatives = exponentialDecay;

        double dt = stepSizes[run];
        int steps = (int)(1.0 / dt);

        for (int i = 0; i < steps; i++) {
            m->getStateDerivatives(m);
            takeStep(m, dt);
        }

        errors[run] = fabs(m->states[0] - analytical);
        freeModel(m);
    }

    /* Error ratio should be ~16 for a 4th-order method (2^4) */
    double ratio = errors[0] / errors[1];
    TEST_ASSERT_DOUBLE_WITHIN(2.0, 16.0, ratio);
}

void test_rungeKutta4_statesRestoredAfterStep(void) {
    /* Verify that after a step, states contain the final result (not intermediate) */
    model = newModel(&ONE_STATE_ONE_PARAM);
    model->states[0] = 1.0;
    ((Params*)model->params)->k = 1.0;
    model->getStateDerivatives = exponentialDecay;

    model->getStateDerivatives(model);
    takeStep(model, 0.1);

    /* Single-step RK4 result — use 1e-6 tolerance for 4th-order error */
    double analytical = exp(-0.1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, analytical, model->states[0]);
}

void test_rungeKutta4_evaluatesTheStagesAtTheirTimes(void) {
    model = newModel(&ONE_STATE);
    model->getStateDerivatives = recordTime;
    model->time = 0.3;
    numberOfEvaluations = 0;

    /* k1 is evaluated by the caller at t = 0.3 */
    takeStep(model, 0.1);

    TEST_ASSERT_EQUAL_INT(3, numberOfEvaluations);
    TEST_ASSERT_EQUAL_DOUBLE(0.3 + 0.05, evaluationTimes[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.3 + 0.05, evaluationTimes[1]);
    TEST_ASSERT_EQUAL_DOUBLE(0.3 + 0.1, evaluationTimes[2]);
    TEST_ASSERT_EQUAL_DOUBLE(0.3 + 0.1, model->time);
}

void test_rungeKutta4_modelWithoutStatesAdvancesTheTime(void) {
    model = newModel(&NO_STATES);
    model->time = 1.0;

    takeStep(model, 0.25);

    TEST_ASSERT_EQUAL_DOUBLE(1.25, model->time);
}

void test_rungeKutta4_orderOfAccuracyForTimeVaryingModel(void) {
    /* dx/dt = cos(t): 4th order only if every stage sees its own time */
    double errors[2];
    double stepSizes[2] = {0.2, 0.1};

    for (int run = 0; run < 2; run++) {
        modelInstance* m = newModel(&ONE_STATE);
        m->getStateDerivatives = cosineOfTime;

        double dt = stepSizes[run];
        int steps = (int)(2.0 / dt + 0.5);

        for (int i = 0; i < steps; i++) {
            m->getStateDerivatives(m);
            takeStep(m, dt);
        }

        errors[run] = fabs(m->states[0] - sin(m->time));
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, m->time);
        freeModel(m);
    }

    double ratio = errors[0] / errors[1];
    TEST_ASSERT_DOUBLE_WITHIN(2.0, 16.0, ratio);
}

void test_rungeKutta4_descriptor(void) {
    TEST_ASSERT_EQUAL_STRING("Runge-Kutta 4", RUNGE_KUTTA_4.name);
    TEST_ASSERT_EQUAL_INT(4, RUNGE_KUTTA_4.order);
    TEST_ASSERT_EQUAL_size_t(5, RUNGE_KUTTA_4.scratchVectors);

    /* k1 ... k4 and the states at the start: allocated by newModel() */
    model = newModel(&TWO_STATES);
    TEST_ASSERT_NOT_NULL(model->scratch);
    TEST_ASSERT_TRUE(model->integrator == &RUNGE_KUTTA_4);
}

// ============ Test Runner ============

int main(void) {
    UNITY_BEGIN();

    // Basic functionality
    RUN_TEST(test_rungeKutta4_singleStateWithConstantDerivative);

    // Edge cases
    RUN_TEST(test_rungeKutta4_zeroStepSize);
    RUN_TEST(test_rungeKutta4_modelWithoutStates);

    // Multiple states
    RUN_TEST(test_rungeKutta4_multipleStates);

    // Validation
    RUN_TEST(test_rungeKutta4_exponentialDecay);
    RUN_TEST(test_rungeKutta4_orderOfAccuracy);
    RUN_TEST(test_rungeKutta4_statesRestoredAfterStep);

    // Time
    RUN_TEST(test_rungeKutta4_evaluatesTheStagesAtTheirTimes);
    RUN_TEST(test_rungeKutta4_modelWithoutStatesAdvancesTheTime);
    RUN_TEST(test_rungeKutta4_orderOfAccuracyForTimeVaryingModel);
    RUN_TEST(test_rungeKutta4_descriptor);

    return UNITY_END();
}
