#include <stdlib.h>
#include "unity.h"
#include "model.h"

/* A test model that uses every variable type and every role */
typedef struct { double gain; int mode; bool enabled; } Params;
typedef struct { double u; bool reset; } Inputs;
typedef struct { double y; int count; } Outputs;
typedef struct { double x1; double x2; } States;

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }
static void integrator(modelInstance* m, double stepSize) { (void)m; (void)stepSize; }

static const modelVariable VARIABLES[] = {
    { PARAM(gain),    .start = 2.5 },
    { PARAM(mode),    .start = 3 },
    { PARAM(enabled), .start = 1 },
    { INPUT(u),       .start = 0.5 },
    { INPUT(reset),   .start = 1 },
    { OUTPUT(y) },
    { OUTPUT(count) },
    { STATE(x1),      .start = 4.0 },
    { STATE(x2),      .start = -1.0 },
};

static const modelDefinition DEFINITION = {
    .name                = "test_model",
    .variables           = VARIABLES,
    .numberOfVariables   = ARRAY_LEN(VARIABLES),
    .sizeOfParams        = sizeof(Params),
    .sizeOfInputs        = sizeof(Inputs),
    .sizeOfOutputs       = sizeof(Outputs),
    .numberOfStates      = NUMBER_OF_DOUBLES(States),
    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrate           = integrator,
};

/* A model without parameters, inputs and outputs */
static const modelDefinition ONLY_STATES = { .numberOfStates = 2 };

static modelInstance* model;
static void* buffer;

void setUp(void) {
    model = newModel(&DEFINITION);
    buffer = malloc(modelDataSize(&DEFINITION));
}

void tearDown(void) {
    free(buffer);
    freeModel(model);
}

/* Give every variable a value that differs from its start value */
static void changeEverything(modelInstance* m) {
    Params*  p   = m->params;
    Inputs*  in  = m->inputs;
    Outputs* out = m->outputs;

    m->time = 100.0;
    p->gain = 99.0;
    p->mode = 98;
    p->enabled = false;
    in->u = 97.0;
    in->reset = false;
    out->y = 96.0;
    out->count = 95;
    m->states[0] = 94.0;
    m->states[1] = 93.0;
    m->stateDerivatives[0] = 92.0;
    m->stateDerivatives[1] = 91.0;
}

void test_modelDataSize_isTheSumOfAllBlocks(void) {
    size_t expected = sizeof(double) + sizeof(Params) + sizeof(Inputs) + sizeof(Outputs) + 2 * sizeof(States);
    TEST_ASSERT_EQUAL_size_t(expected, modelDataSize(&DEFINITION));
}

void test_modelDataSize_handlesEmptyBlocks(void) {
    /* time, 2 states, 2 state derivatives */
    TEST_ASSERT_EQUAL_size_t(5 * sizeof(double), modelDataSize(&ONLY_STATES));
}

void test_modelDataSize_handlesNull(void) {
    TEST_ASSERT_EQUAL_size_t(0, modelDataSize(NULL));
}

void test_restoreModelData_bringsBackEveryBlock(void) {
    const Params*  p   = model->params;
    const Inputs*  in  = model->inputs;
    const Outputs* out = model->outputs;

    /* arrange: snapshot of a changed model, then back to start values */
    changeEverything(model);
    saveModelData(model, buffer);
    resetModel(model);

    /* act */
    restoreModelData(model, buffer);

    /* assert */
    TEST_ASSERT_EQUAL_DOUBLE(100.0, model->time);
    TEST_ASSERT_EQUAL_DOUBLE(99.0, p->gain);
    TEST_ASSERT_EQUAL_INT(98, p->mode);
    TEST_ASSERT_FALSE(p->enabled);
    TEST_ASSERT_EQUAL_DOUBLE(97.0, in->u);
    TEST_ASSERT_FALSE(in->reset);
    TEST_ASSERT_EQUAL_DOUBLE(96.0, out->y);
    TEST_ASSERT_EQUAL_INT(95, out->count);
    TEST_ASSERT_EQUAL_DOUBLE(94.0, model->states[0]);
    TEST_ASSERT_EQUAL_DOUBLE(93.0, model->states[1]);
    TEST_ASSERT_EQUAL_DOUBLE(92.0, model->stateDerivatives[0]);
    TEST_ASSERT_EQUAL_DOUBLE(91.0, model->stateDerivatives[1]);
}

void test_saveModelData_doesNotChangeTheModel(void) {
    const Params* p = model->params;

    saveModelData(model, buffer);

    TEST_ASSERT_EQUAL_DOUBLE(2.5, p->gain);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, model->states[0]);
}

void test_modelData_canBeRestoredIntoAnotherInstance(void) {
    modelInstance* other = newModel(&DEFINITION);
    const Params* p = other->params;

    changeEverything(model);
    saveModelData(model, buffer);
    restoreModelData(other, buffer);

    TEST_ASSERT_EQUAL_DOUBLE(100.0, other->time);
    TEST_ASSERT_EQUAL_DOUBLE(99.0, p->gain);
    TEST_ASSERT_EQUAL_DOUBLE(93.0, other->states[1]);

    freeModel(other);
}

void test_modelData_handlesEmptyBlocks(void) {
    modelInstance* m = newModel(&ONLY_STATES);
    double data[5];

    m->time = 0.5;
    m->states[0] = 1.0;
    m->states[1] = 2.0;
    saveModelData(m, data);
    m->time = 0.0;
    m->states[0] = 0.0;
    m->states[1] = 0.0;
    restoreModelData(m, data);

    TEST_ASSERT_EQUAL_DOUBLE(0.5, m->time);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->states[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->states[1]);

    freeModel(m);
}

void test_modelData_handlesNull(void) {
    saveModelData(NULL, buffer);
    saveModelData(model, NULL);
    restoreModelData(NULL, buffer);
    restoreModelData(model, NULL);
    TEST_ASSERT_TRUE(1);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_modelDataSize_isTheSumOfAllBlocks);
    RUN_TEST(test_modelDataSize_handlesEmptyBlocks);
    RUN_TEST(test_modelDataSize_handlesNull);
    RUN_TEST(test_restoreModelData_bringsBackEveryBlock);
    RUN_TEST(test_saveModelData_doesNotChangeTheModel);
    RUN_TEST(test_modelData_canBeRestoredIntoAnotherInstance);
    RUN_TEST(test_modelData_handlesEmptyBlocks);
    RUN_TEST(test_modelData_handlesNull);

    return UNITY_END();
}
