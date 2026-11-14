#include "unity.h"
#include "model.h"

/* A test model that uses every variable type and every role */
typedef struct { double gain; int mode; bool enabled; double internal; } Params;
typedef struct { double u; bool reset; } Inputs;
typedef struct { double y; int count; } Outputs;
typedef struct { double x1; double x2; } States;

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }
static void integrator(modelInstance* m, double stepSize) { (void)m; (void)stepSize; }

/* Deliberately not grouped by role: the order of the table is free */
static const modelVariable VARIABLES[] = {
    { OUTPUT(y) },
    { PARAM(gain),    .start = 2.5 },
    { STATE(x2),      .start = -1.0 },
    { INPUT(reset),   .start = 1 },
    { PARAM(mode),    .start = 3 },
    { INPUT(u),       .start = 0.5 },
    { STATE_AS(x1, "first_state"), .start = 4.0 },
    { PARAM(enabled), .start = 1 },
    { OUTPUT(count) },
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

/* A model without states */
static const modelDefinition NO_STATES = { .sizeOfParams = sizeof(Params) };

static modelInstance* model;

void setUp(void) {
    model = newModel(&DEFINITION);
}

void tearDown(void) {
    freeModel(model);
}

void test_newModel_allocatesMemory(void) {
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_NOT_NULL(model->params);
    TEST_ASSERT_NOT_NULL(model->inputs);
    TEST_ASSERT_NOT_NULL(model->outputs);
    TEST_ASSERT_NOT_NULL(model->states);
    TEST_ASSERT_NOT_NULL(model->stateDerivatives);
    TEST_ASSERT_NOT_NULL(model->scratch);
}

void test_newModel_returnsNullWithoutDefinition(void) {
    TEST_ASSERT_NULL(newModel(NULL));
}

void test_newModel_keepsDefinition(void) {
    TEST_ASSERT_EQUAL_PTR(&DEFINITION, model->def);
}

void test_newModel_bindsFunctionPointers(void) {
    TEST_ASSERT_TRUE(model->getStateDerivatives == stateEquation);
    TEST_ASSERT_TRUE(model->getOutputs == outputEquation);
    TEST_ASSERT_TRUE(model->integrate == integrator);
}

void test_newModel_appliesStartValuesOfAllTypes(void) {
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    const States* x  = (const States*)model->states;

    TEST_ASSERT_EQUAL_DOUBLE(2.5, p->gain);
    TEST_ASSERT_EQUAL_INT(3, p->mode);
    TEST_ASSERT_TRUE(p->enabled);
    TEST_ASSERT_EQUAL_DOUBLE(0.5, in->u);
    TEST_ASSERT_TRUE(in->reset);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, x->x1);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, x->x2);
}

void test_newModel_zeroesEverythingWithoutStartValue(void) {
    const Params*  p   = model->params;
    const Outputs* out = model->outputs;

    TEST_ASSERT_EQUAL_DOUBLE(0.0, p->internal);  /* not published */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, out->y);
    TEST_ASSERT_EQUAL_INT(0, out->count);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, model->stateDerivatives[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, model->stateDerivatives[1]);
}

void test_newModel_statesAreAVectorOfDoubles(void) {
    /* The struct States is a view on the state vector */
    TEST_ASSERT_EQUAL_DOUBLE(4.0, model->states[0]);
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, model->states[1]);
}

void test_newModel_handlesEmptyBlocks(void) {
    modelInstance* m = newModel(&ONLY_STATES);

    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_NULL(m->params);
    TEST_ASSERT_NULL(m->inputs);
    TEST_ASSERT_NULL(m->outputs);
    TEST_ASSERT_NOT_NULL(m->states);

    freeModel(m);
}

void test_newModel_handlesModelWithoutStates(void) {
    modelInstance* m = newModel(&NO_STATES);

    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_NOT_NULL(m->params);
    TEST_ASSERT_NULL(m->states);
    TEST_ASSERT_NULL(m->stateDerivatives);
    TEST_ASSERT_NULL(m->scratch);

    freeModel(m);
}

void test_resetModel_restoresStartValues(void) {
    Params*  p   = model->params;
    Inputs*  in  = model->inputs;
    Outputs* out = model->outputs;

    /* arrange: change everything */
    p->gain = 99.0;
    p->mode = 99;
    p->enabled = false;
    p->internal = 99.0;
    in->u = 99.0;
    out->count = 99;
    model->states[0] = 99.0;
    model->stateDerivatives[1] = 99.0;
    model->integrate = NULL;

    /* act */
    resetModel(model);

    /* assert */
    TEST_ASSERT_EQUAL_DOUBLE(2.5, p->gain);
    TEST_ASSERT_EQUAL_INT(3, p->mode);
    TEST_ASSERT_TRUE(p->enabled);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, p->internal);
    TEST_ASSERT_EQUAL_DOUBLE(0.5, in->u);
    TEST_ASSERT_EQUAL_INT(0, out->count);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, model->states[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, model->stateDerivatives[1]);
    TEST_ASSERT_TRUE(model->integrate == integrator);
}

void test_resetModel_handlesNull(void) {
    resetModel(NULL);
    TEST_ASSERT_TRUE(1);
}

void test_variableMacros_deriveNameTypeAndRole(void) {
    /* { PARAM(mode) } is entry 4 of the table */
    TEST_ASSERT_EQUAL_STRING("mode", VARIABLES[4].name);
    TEST_ASSERT_EQUAL_INT(VAR_INTEGER, VARIABLES[4].type);
    TEST_ASSERT_EQUAL_INT(VAR_PARAMETER, VARIABLES[4].role);

    /* { INPUT(reset) } is entry 3 */
    TEST_ASSERT_EQUAL_INT(VAR_BOOLEAN, VARIABLES[3].type);
    TEST_ASSERT_EQUAL_INT(VAR_INPUT, VARIABLES[3].role);

    /* { STATE_AS(x1, "first_state") } is entry 6 */
    TEST_ASSERT_EQUAL_STRING("first_state", VARIABLES[6].name);
    TEST_ASSERT_EQUAL_INT(VAR_REAL, VARIABLES[6].type);
    TEST_ASSERT_EQUAL_INT(VAR_STATE, VARIABLES[6].role);
}

void test_modelVariableAddress_pointsToTheStructField(void) {
    Params*  p   = model->params;
    Inputs*  in  = model->inputs;
    Outputs* out = model->outputs;

    TEST_ASSERT_EQUAL_PTR(&out->y,           modelVariableAddress(model, 0));
    TEST_ASSERT_EQUAL_PTR(&p->gain,          modelVariableAddress(model, 1));
    TEST_ASSERT_EQUAL_PTR(&model->states[1], modelVariableAddress(model, 2));
    TEST_ASSERT_EQUAL_PTR(&in->reset,        modelVariableAddress(model, 3));
    TEST_ASSERT_EQUAL_PTR(&p->mode,          modelVariableAddress(model, 4));
    TEST_ASSERT_EQUAL_PTR(&model->states[0], modelVariableAddress(model, 6));
    TEST_ASSERT_EQUAL_PTR(&out->count,       modelVariableAddress(model, 8));
}

void test_modelVariableAddress_rejectsInvalidArguments(void) {
    TEST_ASSERT_NULL(modelVariableAddress(model, ARRAY_LEN(VARIABLES)));
    TEST_ASSERT_NULL(modelVariableAddress(NULL, 0));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_newModel_allocatesMemory);
    RUN_TEST(test_newModel_returnsNullWithoutDefinition);
    RUN_TEST(test_newModel_keepsDefinition);
    RUN_TEST(test_newModel_bindsFunctionPointers);
    RUN_TEST(test_newModel_appliesStartValuesOfAllTypes);
    RUN_TEST(test_newModel_zeroesEverythingWithoutStartValue);
    RUN_TEST(test_newModel_statesAreAVectorOfDoubles);
    RUN_TEST(test_newModel_handlesEmptyBlocks);
    RUN_TEST(test_newModel_handlesModelWithoutStates);
    RUN_TEST(test_resetModel_restoresStartValues);
    RUN_TEST(test_resetModel_handlesNull);
    RUN_TEST(test_variableMacros_deriveNameTypeAndRole);
    RUN_TEST(test_modelVariableAddress_pointsToTheStructField);
    RUN_TEST(test_modelVariableAddress_rejectsInvalidArguments);

    return UNITY_END();
}
