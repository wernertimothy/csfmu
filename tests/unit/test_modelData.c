#include <stdlib.h>
#include "unity.h"
#include "model.h"
#include "integrator.h"

/* A test model that uses every variable type and every role */
typedef struct { double gain; int mode; bool enabled; } Params;
typedef struct { double u; bool reset; } Inputs;
typedef struct { double y; int count; } Outputs;
typedef struct { double ratio; int level; bool saturated; } Locals;
typedef struct { double x1; double x2; } States;
typedef struct { double cache; int counter; bool flag; double table[4]; } Internals;

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }

static const modelVariable VARIABLES[] = {
    { PARAM(gain),    .start = 2.5 },
    { PARAM(mode),    .start = 3 },
    { PARAM(enabled), .start = 1 },
    { INPUT(u),       .start = 0.5 },
    { INPUT(reset),   .start = 1 },
    { OUTPUT(y) },
    { OUTPUT(count) },
    { LOCAL(ratio) },
    { LOCAL(level) },
    { LOCAL(saturated) },
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
    .sizeOfLocals        = sizeof(Locals),
    .sizeOfInternals     = sizeof(Internals),
    .numberOfStates      = NUMBER_OF_DOUBLES(States),
    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrator          = &FORWARD_EULER,
};

/* A model without parameters, inputs, outputs, locals and internals */
static const modelDefinition ONLY_STATES = { .numberOfStates = 2 };

/* A model that consists of nothing but its internals */
static const modelDefinition ONLY_INTERNALS = { .sizeOfInternals = sizeof(Internals) };

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
    Locals*  locals = m->locals;
    Internals* internals = m->internals;

    m->time = 100.0;
    m->relTol = 1e-3;
    p->gain = 99.0;
    p->mode = 98;
    p->enabled = false;
    in->u = 97.0;
    in->reset = false;
    out->y = 96.0;
    out->count = 95;
    locals->ratio = 87.0;
    locals->level = 86;
    locals->saturated = true;
    m->states[0] = 94.0;
    m->states[1] = 93.0;
    m->stateDerivatives[0] = 92.0;
    m->stateDerivatives[1] = 91.0;
    internals->cache = 90.0;
    internals->counter = 89;
    internals->flag = true;
    for (size_t k = 0; k < ARRAY_LEN(internals->table); k++) {
        internals->table[k] = 88.0 - (double)k;
    }
    m->initialGuess[0] = 85.0;
    m->initialGuess[1] = 84.0;
    m->hasInitialGuess = true;
}

/* The internals as changeEverything() leaves them */
static void assertInternalsChanged(const modelInstance* m) {
    const Internals* internals = m->internals;

    TEST_ASSERT_EQUAL_DOUBLE(90.0, internals->cache);
    TEST_ASSERT_EQUAL_INT(89, internals->counter);
    TEST_ASSERT_TRUE(internals->flag);
    for (size_t k = 0; k < ARRAY_LEN(internals->table); k++) {
        TEST_ASSERT_EQUAL_DOUBLE(88.0 - (double)k, internals->table[k]);
    }
}

void test_modelDataSize_isTheSumOfAllBlocks(void) {
    /* time, tolerance, the blocks, states, state derivatives, initial guesses and hasInitialGuess */
    size_t expected = 2 * sizeof(double) + sizeof(Params) + sizeof(Inputs) + sizeof(Outputs) + 3 * sizeof(States) +
                      sizeof(Internals) + sizeof(Locals) + sizeof(bool);
    TEST_ASSERT_EQUAL_size_t(expected, modelDataSize(&DEFINITION));
}

void test_modelDataSize_handlesEmptyBlocks(void) {
    /* time, tolerance, 2 states, 2 state derivatives, 2 initial guesses and hasInitialGuess */
    TEST_ASSERT_EQUAL_size_t(8 * sizeof(double) + sizeof(bool), modelDataSize(&ONLY_STATES));
}

void test_modelDataSize_handlesNull(void) {
    TEST_ASSERT_EQUAL_size_t(0, modelDataSize(NULL));
}

void test_restoreModelData_bringsBackEveryBlock(void) {
    const Params*  p   = model->params;
    const Inputs*  in  = model->inputs;
    const Outputs* out = model->outputs;
    const Locals*  locals = model->locals;

    /* arrange: snapshot of a changed model, then back to start values */
    changeEverything(model);
    saveModelData(model, buffer);
    resetModel(model);

    /* act */
    restoreModelData(model, buffer);

    /* assert */
    TEST_ASSERT_EQUAL_DOUBLE(100.0, model->time);
    TEST_ASSERT_EQUAL_DOUBLE(1e-3, model->relTol);
    TEST_ASSERT_EQUAL_DOUBLE(99.0, p->gain);
    TEST_ASSERT_EQUAL_INT(98, p->mode);
    TEST_ASSERT_FALSE(p->enabled);
    TEST_ASSERT_EQUAL_DOUBLE(97.0, in->u);
    TEST_ASSERT_FALSE(in->reset);
    TEST_ASSERT_EQUAL_DOUBLE(96.0, out->y);
    TEST_ASSERT_EQUAL_INT(95, out->count);
    TEST_ASSERT_EQUAL_DOUBLE(87.0, locals->ratio);
    TEST_ASSERT_EQUAL_INT(86, locals->level);
    TEST_ASSERT_TRUE(locals->saturated);
    TEST_ASSERT_EQUAL_DOUBLE(94.0, model->states[0]);
    TEST_ASSERT_EQUAL_DOUBLE(93.0, model->states[1]);
    TEST_ASSERT_EQUAL_DOUBLE(92.0, model->stateDerivatives[0]);
    TEST_ASSERT_EQUAL_DOUBLE(91.0, model->stateDerivatives[1]);
    assertInternalsChanged(model);
    TEST_ASSERT_EQUAL_DOUBLE(85.0, model->initialGuess[0]);
    TEST_ASSERT_EQUAL_DOUBLE(84.0, model->initialGuess[1]);
    TEST_ASSERT_TRUE(model->hasInitialGuess);
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
    TEST_ASSERT_EQUAL_DOUBLE(1e-3, other->relTol);
    TEST_ASSERT_EQUAL_DOUBLE(99.0, p->gain);
    TEST_ASSERT_EQUAL_DOUBLE(93.0, other->states[1]);
    TEST_ASSERT_EQUAL_INT(86, ((const Locals*)other->locals)->level);
    assertInternalsChanged(other);
    TEST_ASSERT_EQUAL_DOUBLE(84.0, other->initialGuess[1]);
    TEST_ASSERT_TRUE(other->hasInitialGuess);

    freeModel(other);
}

void test_resetModel_zeroesTheInternals(void) {
    const Internals* internals = model->internals;

    changeEverything(model);
    resetModel(model);

    TEST_ASSERT_EQUAL_DOUBLE(0.0, internals->cache);
    TEST_ASSERT_EQUAL_INT(0, internals->counter);
    TEST_ASSERT_FALSE(internals->flag);
    for (size_t k = 0; k < ARRAY_LEN(internals->table); k++) {
        TEST_ASSERT_EQUAL_DOUBLE(0.0, internals->table[k]);
    }
}

void test_resetModel_keepsTheInternalsBlock(void) {
    /* The block is zeroed in place, so pointers to it stay valid */
    void* before = model->internals;

    resetModel(model);

    TEST_ASSERT_EQUAL_PTR(before, model->internals);
}

void test_modelData_handlesAModelWithOnlyInternals(void) {
    modelInstance* m = newModel(&ONLY_INTERNALS);
    void* data = malloc(modelDataSize(&ONLY_INTERNALS));
    Internals* internals = m->internals;

    /* time, tolerance, the internals and hasInitialGuess */
    TEST_ASSERT_EQUAL_size_t(2 * sizeof(double) + sizeof(Internals) + sizeof(bool), modelDataSize(&ONLY_INTERNALS));

    internals->counter = 7;
    internals->table[3] = 0.25;
    saveModelData(m, data);
    resetModel(m);
    TEST_ASSERT_EQUAL_INT(0, internals->counter);
    restoreModelData(m, data);

    TEST_ASSERT_EQUAL_INT(7, internals->counter);
    TEST_ASSERT_EQUAL_DOUBLE(0.25, internals->table[3]);

    free(data);
    freeModel(m);
}

void test_modelData_handlesEmptyBlocks(void) {
    modelInstance* m = newModel(&ONLY_STATES);
    void* data = malloc(modelDataSize(&ONLY_STATES));

    TEST_ASSERT_NULL(m->internals);
    TEST_ASSERT_NULL(m->locals);

    m->time = 0.5;
    m->relTol = 1e-9;
    m->states[0] = 1.0;
    m->states[1] = 2.0;
    saveModelData(m, data);
    m->time = 0.0;
    m->relTol = 0.0;
    m->states[0] = 0.0;
    m->states[1] = 0.0;
    restoreModelData(m, data);

    TEST_ASSERT_EQUAL_DOUBLE(0.5, m->time);
    TEST_ASSERT_EQUAL_DOUBLE(1e-9, m->relTol);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, m->states[0]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, m->states[1]);

    free(data);
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
    RUN_TEST(test_resetModel_zeroesTheInternals);
    RUN_TEST(test_resetModel_keepsTheInternalsBlock);
    RUN_TEST(test_modelData_handlesAModelWithOnlyInternals);
    RUN_TEST(test_modelData_handlesEmptyBlocks);
    RUN_TEST(test_modelData_handlesNull);

    return UNITY_END();
}
