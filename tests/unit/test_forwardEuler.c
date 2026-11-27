#include "unity.h"
#include "model.h"
#include "integrator.h"
#include <math.h>

#define TOLERANCE 1e-10

/* Minimal model definitions: the number of states and the integrator, which sizes the work memory */
static const modelDefinition ONE_STATE    = { .numberOfStates = 1, .integrator = &FORWARD_EULER };
static const modelDefinition THREE_STATES = { .numberOfStates = 3, .integrator = &FORWARD_EULER };

static modelInstance* model;

/* One step of FORWARD_EULER: it succeeds and takes the whole step (fixed-step contract) */
static void takeStep(modelInstance* m, double h)
{
    double taken = NAN;
    TEST_ASSERT_EQUAL_INT(SIM_OK, FORWARD_EULER.step(m, h, &taken));
    TEST_ASSERT_EQUAL_DOUBLE(h, taken);
}

void setUp(void) {
    model = NULL;
}

void tearDown(void) {
    freeModel(model);
}

void test_forwardEuler_singleStateWithConstantDerivative(void) {
    model = newModel(&ONE_STATE);
    model->stateDerivatives[0] = 2.0;
    takeStep(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.2, model->states[0]);
}

void test_forwardEuler_singleStateWithZeroDerivative(void) {
    model = newModel(&ONE_STATE);
    model->states[0] = 2.0;
    takeStep(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 2.0, model->states[0]);
}

void test_forwardEuler_singleStateWithNegativeDerivative(void) {
    model = newModel(&ONE_STATE);
    model->stateDerivatives[0] = -2.0;
    takeStep(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, -0.2, model->states[0]);
}

void test_forwardEuler_zeroStepSize(void) {
    model = newModel(&ONE_STATE);
    model->states[0] = 5.0;
    model->stateDerivatives[0] = 1.0;
    takeStep(model, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, model->states[0]);
}

void test_forwardEuler_multipleStates(void) {
      model = newModel(&THREE_STATES);
      model->states[0] = 1.0;
      model->states[1] = 2.0;
      model->states[2] = 3.0;
      model->stateDerivatives[0] = 1.0;
      model->stateDerivatives[1] = 2.0;
      model->stateDerivatives[2] = 3.0;

      takeStep(model, 0.1);

      TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 1.1, model->states[0]);
      TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 2.2, model->states[1]);
      TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 3.3, model->states[2]);
}

void test_forwardEuler_variableStepSize(void) {
    model = newModel(&ONE_STATE);
    model->states[0] = 1.0;
    model->stateDerivatives[0] = 2.0;

    takeStep(model, 0.1);
    takeStep(model, 0.2);

    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 1.6, model->states[0]);
}

void test_forwardEuler_multipleSteps(void) {
    model = newModel(&ONE_STATE);
    model->states[0] = 0.0;
    model->stateDerivatives[0] = 1.0;

    // Step 1
    takeStep(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.1, model->states[0]);

    // Step 2
    takeStep(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.2, model->states[0]);

    // Step 3
    takeStep(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.3, model->states[0]);
}

void test_forwardEuler_exponentialDecay(void) {
    // Setup: dx/dt = -1.0 * x  (k = 1.0)
    model = newModel(&ONE_STATE);
    model->states[0] = 1.0;  // Initial condition

    double k = 1.0;
    double dt = 0.01;     // Small step for accuracy
    double T = 1.0;       // Simulate to t=1
    int steps = (int)(T / dt);

    for (int i = 0; i < steps; i++) {
        // Compute derivative manually: dx/dt = -k*x
        model->stateDerivatives[0] = -k * model->states[0];

        // Integrate
        takeStep(model, dt);
    }

    // Analytical solution: x(1) = 1.0 * exp(-1.0 * 1.0) ≈ 0.36788
    double analytical = exp(-k * T);

    // Forward Euler has discretization error, so use loose tolerance
    TEST_ASSERT_DOUBLE_WITHIN(0.01, analytical, model->states[0]);
}

void test_forwardEuler_advancesTheTime(void) {
    model = newModel(&ONE_STATE);
    model->time = 0.3;
    model->stateDerivatives[0] = 1.0;

    takeStep(model, 0.1);

    TEST_ASSERT_EQUAL_DOUBLE(0.3 + 0.1, model->time);
}

void test_forwardEuler_descriptor(void) {
    TEST_ASSERT_EQUAL_STRING("Forward Euler", FORWARD_EULER.name);
    TEST_ASSERT_EQUAL_INT(1, FORWARD_EULER.order);
    TEST_ASSERT_EQUAL_size_t(0, FORWARD_EULER.scratchVectors);

    /* no work memory needed, so none is allocated */
    model = newModel(&ONE_STATE);
    TEST_ASSERT_NULL(model->scratch);
    TEST_ASSERT_TRUE(model->integrator == &FORWARD_EULER);
}

// ============ Test Runner ============

int main(void) {
    UNITY_BEGIN();

    // Basic functionality
    RUN_TEST(test_forwardEuler_singleStateWithConstantDerivative);
    RUN_TEST(test_forwardEuler_singleStateWithZeroDerivative);
    RUN_TEST(test_forwardEuler_singleStateWithNegativeDerivative);

    // Edge cases
    RUN_TEST(test_forwardEuler_zeroStepSize);

    // Multiple states
    RUN_TEST(test_forwardEuler_multipleStates);

    // Behavioral
    RUN_TEST(test_forwardEuler_multipleSteps);
    RUN_TEST(test_forwardEuler_variableStepSize);
    RUN_TEST(test_forwardEuler_advancesTheTime);

    // Validation
    RUN_TEST(test_forwardEuler_exponentialDecay);
    RUN_TEST(test_forwardEuler_descriptor);

    return UNITY_END();
}