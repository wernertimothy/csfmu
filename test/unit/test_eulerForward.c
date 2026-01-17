#include "unity.h"
#include "model.h"
#include "integrator.h"
#include <math.h>

#define TOLERANCE 1e-10

static modelInstance* model;

void setUp(void) {
}

void tearDown(void) {
    freeModel(model);
}

void test_eulerForward_singleStateWithConstantDerivative(void) {
    model = newModel(1,0,0,0);
    model->stateDerivatives[0] = 2.0;
    eulerForward(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.2, model->states[0]);
}

void test_eulerForward_singleStateWithZeroDerivative(void) {
    model = newModel(1,0,0,0);
    model->states[0] = 2.0;
    eulerForward(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 2.0, model->states[0]);
}

void test_eulerForward_singleStateWithNegativeDerivative(void) {
    model = newModel(1,0,0,0);
    model->stateDerivatives[0] = -2.0;
    eulerForward(model, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, -0.2, model->states[0]);
}

void test_eulerForward_zeroStepSize(void) {
    model = newModel(1,0,0,0);
    model->states[0] = 5.0;
    model->stateDerivatives[0] = 1.0;
    eulerForward(model, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, model->states[0]);
}

void test_eulerForward_multipleStates(void) {
      modelInstance* m = newModel(3, 0, 0, 0);
      m->states[0] = 1.0;
      m->states[1] = 2.0;
      m->states[2] = 3.0;
      m->stateDerivatives[0] = 1.0;
      m->stateDerivatives[1] = 2.0;
      m->stateDerivatives[2] = 3.0;

      eulerForward(m, 0.1);

      TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 1.1, m->states[0]);
      TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 2.2, m->states[1]);
      TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 3.3, m->states[2]);
}

void test_eulerForward_variableStepSize(void) {
    model = newModel(1,0,0,0);
    model->states[0] = 1.0;
    model->stateDerivatives[0] = 2.0;

    eulerForward(model, 0.1);
    eulerForward(model, 0.2);

    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 1.6, model->states[0]);
}

void test_eulerForward_multipleSteps(void) {
    modelInstance* m = newModel(1, 0, 0, 0);
    m->states[0] = 0.0;
    m->stateDerivatives[0] = 1.0;

    // Step 1
    eulerForward(m, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.1, m->states[0]);

    // Step 2
    eulerForward(m, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.2, m->states[0]);

    // Step 3
    eulerForward(m, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(TOLERANCE, 0.3, m->states[0]);
}

void test_eulerForward_exponentialDecay(void) {
    // Setup: dx/dt = -1.0 * x  (k = 1.0)
    modelInstance* m = newModel(1, 0, 0, 0);
    m->states[0] = 1.0;  // Initial condition

    double k = 1.0;
    double dt = 0.01;     // Small step for accuracy
    double T = 1.0;       // Simulate to t=1
    int steps = (int)(T / dt);

    for (int i = 0; i < steps; i++) {
        // Compute derivative manually: dx/dt = -k*x
        m->stateDerivatives[0] = -k * m->states[0];

        // Integrate
        eulerForward(m, dt);
    }

    // Analytical solution: x(1) = 1.0 * exp(-1.0 * 1.0) ≈ 0.36788
    double analytical = exp(-k * T);

    // Euler forward has discretization error, so use loose tolerance
    TEST_ASSERT_DOUBLE_WITHIN(0.01, analytical, m->states[0]);
}

// ============ Test Runner ============

int main(void) {
    UNITY_BEGIN();

    // Basic functionality
    RUN_TEST(test_eulerForward_singleStateWithConstantDerivative);
    RUN_TEST(test_eulerForward_singleStateWithZeroDerivative);
    RUN_TEST(test_eulerForward_singleStateWithNegativeDerivative);

    // Edge cases
    RUN_TEST(test_eulerForward_zeroStepSize);

    // Multiple states
    RUN_TEST(test_eulerForward_multipleStates);

    // Behavioral
    RUN_TEST(test_eulerForward_multipleSteps);
    RUN_TEST(test_eulerForward_variableStepSize);

    // Validation
    RUN_TEST(test_eulerForward_exponentialDecay);

    return UNITY_END();
}