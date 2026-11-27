/**
 * Test of the initialization in the model layer: initializeModel(),
 * beginInitialization(), computeCalculatedParameters() and modelError().
 * No FMI involved.
 */

#include <string.h>
#include "unity.h"
#include "model.h"
#include "integrator.h"

typedef struct { double gain; double twice; } Params;
typedef struct { double x; double y; } States;
typedef struct { int initializeCalls; double twiceSeenByInitialize; } Internals;

/* What the test model's initialize function returns */
static simStatus initializeResult = SIM_OK;

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }

static void calculateParameters(modelInstance* m)
{
    Params* p = m->params;
    p->twice = 2.0 * p->gain;
}

/* Moves the guess x by 1 from wherever it starts, so a call that does not
   start from the guess moves it further; leaves y (initial = exact) alone */
static simStatus initialize(modelInstance* m)
{
    const Params* p = m->params;
    Internals* internals = m->internals;

    internals->initializeCalls++;
    internals->twiceSeenByInitialize = p->twice;
    m->states[0] += 1.0;

    if (initializeResult == SIM_ERROR) {
        return modelError(m, "gain %g is not allowed", p->gain);
    }
    return initializeResult;
}

static const modelVariable VARIABLES[] = {
    { PARAM(gain),            .start = 1.5 },
    { CALCULATED_PARAM(twice) },
    { STATE(x),               .initial = VAR_APPROX },
    { STATE(y) },
};

static const modelDefinition DEFINITION = {
    .name                = "initialize_model",
    .variables           = VARIABLES,
    .numberOfVariables   = ARRAY_LEN(VARIABLES),
    .sizeOfParams        = sizeof(Params),
    .sizeOfInternals     = sizeof(Internals),
    .numberOfStates      = NUMBER_OF_DOUBLES(States),
    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrator          = &FORWARD_EULER,
    .calculateParameters = calculateParameters,
    .initialize          = initialize,
};

/* The same model without the two hooks */
static const modelDefinition WITHOUT_HOOKS = {
    .name                = "initialize_model",
    .variables           = VARIABLES,
    .numberOfVariables   = ARRAY_LEN(VARIABLES),
    .sizeOfParams        = sizeof(Params),
    .sizeOfInternals     = sizeof(Internals),
    .numberOfStates      = NUMBER_OF_DOUBLES(States),
    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrator          = &FORWARD_EULER,
};

static modelInstance* model;

void setUp(void)
{
    initializeResult = SIM_OK;
    model = newModel(&DEFINITION);
}

void tearDown(void)
{
    freeModel(model);
}

void test_initializeModel_computesCalculatedParametersBeforeInitialize(void)
{
    const Params* p = model->params;
    const Internals* internals = model->internals;

    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));

    TEST_ASSERT_EQUAL_DOUBLE(3.0, p->twice);
    TEST_ASSERT_EQUAL_INT(1, internals->initializeCalls);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, internals->twiceSeenByInitialize);
}

void test_initializeModel_passesOnTheStatusOfInitialize(void)
{
    initializeResult = SIM_DISCARD;
    TEST_ASSERT_EQUAL_INT(SIM_DISCARD, initializeModel(model));

    initializeResult = SIM_ERROR;
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, initializeModel(model));
    TEST_ASSERT_EQUAL_STRING("gain 1.5 is not allowed", model->message);
}

void test_initializeModel_worksWithoutHooks(void)
{
    modelInstance* m = newModel(&WITHOUT_HOOKS);
    const Params* p = m->params;

    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(m));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, p->twice);

    freeModel(m);
}

void test_initializeModel_handlesNull(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, initializeModel(NULL));
}

/***************************************************
 * Initial guesses
 ***************************************************/

void test_initializeModel_startsFromTheCurrentStates(void)
{
    /* a single call needs no guesses: it starts from the states as they are */
    model->states[0] = 3.0;

    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));

    TEST_ASSERT_EQUAL_DOUBLE(4.0, model->states[0]);
}

void test_initializeModel_startsEveryLaterCallFromTheGuess(void)
{
    const Internals* internals = model->internals;
    model->states[0] = 3.0;

    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));
    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));
    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));

    TEST_ASSERT_EQUAL_DOUBLE(4.0, model->states[0]);  /* not 6.0 */
    TEST_ASSERT_EQUAL_INT(3, internals->initializeCalls);
}

void test_initializeModel_keepsTheExactStatesTheCallerSet(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));
    model->states[1] = 7.0;  /* y has initial = exact: the caller may still set it */

    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));

    TEST_ASSERT_EQUAL_DOUBLE(7.0, model->states[1]);
}

void test_initializeModel_keepsTheGuessesAfterAFailure(void)
{
    model->states[0] = 3.0;
    initializeResult = SIM_ERROR;
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, initializeModel(model));

    initializeResult = SIM_OK;
    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));

    TEST_ASSERT_EQUAL_DOUBLE(4.0, model->states[0]);
}

void test_beginInitialization_takesNewGuesses(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));  /* guess x = 0 */

    beginInitialization(model);
    TEST_ASSERT_FALSE(model->hasInitialGuess);
    model->states[0] = 10.0;
    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));

    TEST_ASSERT_EQUAL_DOUBLE(11.0, model->states[0]);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, model->initialGuess[0]);
}

void test_resetModel_forgetsTheGuesses(void)
{
    model->states[0] = 3.0;
    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));

    resetModel(model);

    TEST_ASSERT_FALSE(model->hasInitialGuess);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, model->initialGuess[0]);
    model->states[0] = 7.0;
    TEST_ASSERT_EQUAL_INT(SIM_OK, initializeModel(model));
    TEST_ASSERT_EQUAL_DOUBLE(8.0, model->states[0]);
}

void test_beginInitialization_handlesNull(void)
{
    beginInitialization(NULL);
    TEST_ASSERT_TRUE(1);
}

void test_computeCalculatedParameters_followsTheParameters(void)
{
    Params* p = model->params;

    p->gain = 4.0;
    computeCalculatedParameters(model);
    TEST_ASSERT_EQUAL_DOUBLE(8.0, p->twice);

    /* it has no side effects, so it can be called again */
    computeCalculatedParameters(model);
    TEST_ASSERT_EQUAL_DOUBLE(8.0, p->twice);
    TEST_ASSERT_EQUAL_INT(0, ((const Internals*)model->internals)->initializeCalls);

    computeCalculatedParameters(NULL);
}

void test_calculatedParameter_livesInTheParameterBlock(void)
{
    const Params* p = model->params;

    TEST_ASSERT_EQUAL_INT(VAR_CALCULATED_PARAMETER, VARIABLES[1].role);
    TEST_ASSERT_EQUAL_INT(VAR_REAL, VARIABLES[1].type);
    TEST_ASSERT_EQUAL_STRING("twice", VARIABLES[1].name);
    TEST_ASSERT_EQUAL_PTR(&p->twice, modelVariableAddress(model, 1));
}

void test_modelError_formatsTheMessage(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, modelError(model, "file '%s' has %d lines", "table.csv", 3));
    TEST_ASSERT_EQUAL_STRING("file 'table.csv' has 3 lines", model->message);
}

void test_modelError_truncatesALongMessage(void)
{
    char longText[2 * MODEL_MESSAGE_SIZE];
    memset(longText, 'a', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';

    modelError(model, "%s", longText);

    TEST_ASSERT_EQUAL_size_t(MODEL_MESSAGE_SIZE - 1, strlen(model->message));
}

void test_modelError_handlesNull(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, modelError(NULL, "ignored"));
}

void test_resetModel_clearsTheMessage(void)
{
    modelError(model, "something went wrong");

    resetModel(model);

    TEST_ASSERT_EQUAL_STRING("", model->message);
}

void test_newModel_startsWithoutMessage(void)
{
    TEST_ASSERT_EQUAL_STRING("", model->message);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initializeModel_computesCalculatedParametersBeforeInitialize);
    RUN_TEST(test_initializeModel_passesOnTheStatusOfInitialize);
    RUN_TEST(test_initializeModel_worksWithoutHooks);
    RUN_TEST(test_initializeModel_handlesNull);
    RUN_TEST(test_initializeModel_startsFromTheCurrentStates);
    RUN_TEST(test_initializeModel_startsEveryLaterCallFromTheGuess);
    RUN_TEST(test_initializeModel_keepsTheExactStatesTheCallerSet);
    RUN_TEST(test_initializeModel_keepsTheGuessesAfterAFailure);
    RUN_TEST(test_beginInitialization_takesNewGuesses);
    RUN_TEST(test_resetModel_forgetsTheGuesses);
    RUN_TEST(test_beginInitialization_handlesNull);
    RUN_TEST(test_computeCalculatedParameters_followsTheParameters);
    RUN_TEST(test_calculatedParameter_livesInTheParameterBlock);
    RUN_TEST(test_modelError_formatsTheMessage);
    RUN_TEST(test_modelError_truncatesALongMessage);
    RUN_TEST(test_modelError_handlesNull);
    RUN_TEST(test_resetModel_clearsTheMessage);
    RUN_TEST(test_newModel_startsWithoutMessage);
    return UNITY_END();
}
