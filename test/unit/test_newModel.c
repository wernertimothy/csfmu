#include "unity.h"
#include "model.h"

static modelInstance* model;
static size_t nx; // state dimesnion
static size_t nu; // input dimension
static size_t ny; // output dimension
static size_t np; // number of parameter

void setUp(void) {
    /* arrange */
    nx = 2;
    nu = 3;
    ny = 4;
    np = 5;

    /* act */
    model = newModel(nx, nu, ny, np);
}

void tearDown(void) {
    freeModel(model);
}

void test_newModel_allocatesMemory(void) {
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_NOT_NULL(model->states);
    TEST_ASSERT_NOT_NULL(model->inputs);
    TEST_ASSERT_NOT_NULL(model->outputs);
    TEST_ASSERT_NOT_NULL(model->parameter);
    TEST_ASSERT_NOT_NULL(model->stateDerivatives);
    TEST_ASSERT_NOT_NULL(model->info);

}

void test_newModel_initilizesAllArraysToZero(void) {
    size_t i = 0;
    for (i=0; i<nx; i++) {
        TEST_ASSERT_EQUAL_DOUBLE(0.0, model->states[i]);
        TEST_ASSERT_EQUAL_DOUBLE(0.0, model->stateDerivatives[i]);
    }
    for (i=0; i<nu; i++) {
        TEST_ASSERT_EQUAL_DOUBLE(0.0, model->inputs[i]);
    }
    for (i=0; i<ny; i++) {
        TEST_ASSERT_EQUAL_DOUBLE(0.0, model->outputs[i]);
    }
    for (i=0; i<np; i++) {
        TEST_ASSERT_EQUAL_DOUBLE(0.0, model->parameter[i]);
    }
}

void test_newModel_initializesInfo(void) {
    TEST_ASSERT_EQUAL_size_t(nx, model->info->numberOfStates);
    TEST_ASSERT_EQUAL_size_t(nu, model->info->numberOfInputs);
    TEST_ASSERT_EQUAL_size_t(ny, model->info->numberOfOutputs);
    TEST_ASSERT_EQUAL_size_t(np, model->info->numberOfParameter);

}

void test_newModel_initilizesOpaqueFunctionPointer(void) {
    TEST_ASSERT_NULL(model->getStateDerivatives);
    TEST_ASSERT_NULL(model->getOutputs);
    TEST_ASSERT_NULL(model->integrate);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_newModel_allocatesMemory);
    RUN_TEST(test_newModel_initilizesAllArraysToZero);
    RUN_TEST(test_newModel_initializesInfo);
    RUN_TEST(test_newModel_initilizesOpaqueFunctionPointer);

    return UNITY_END();
}