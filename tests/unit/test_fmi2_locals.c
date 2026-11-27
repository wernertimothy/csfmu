/**
 * Test of local variables (VAR_LOCAL) through the FMI wrapper: they are
 * computed by the output equation, can be read with every getter, can never
 * be set, follow a set without a step (like outputs), and a non-finite Real
 * local discards the step.
 *
 * The FMI wrapper (src/fmi2Functions.c) is linked against the model
 * definition below instead of a real model, as in
 * test_fmi2_integratorContract.c, so the test is built for every model and
 * runs in the sanitizer job.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "fmi2Functions.h"
#include "model.h"
#include "integrator.h"

#define STEP_SIZE 0.1

/***************************************************
 * Test model: dx/dt = -x, y = x, and three locals computed from x
 ***************************************************/

typedef struct { double y; } Outputs;
typedef struct { double ratio; int percent; bool positive; } Locals;
typedef struct { double x; } States;

/* The output equation makes the local ratio NaN */
static bool poisonRatio = false;

static void stateEquation(modelInstance* m)
{
    m->stateDerivatives[0] = -m->states[0];
}

static void outputEquation(modelInstance* m)
{
    double x = m->states[0];
    Outputs* out = m->outputs;
    Locals* locals = m->locals;

    out->y = x;
    locals->ratio = poisonRatio ? NAN : x / 2.0;
    locals->percent = (int)lround(100.0 * x);
    locals->positive = x > 0.0;
}

static const modelVariable VARIABLES[] = {
    { OUTPUT(y),       .dependsOn = "", .description = "The state" },
    { LOCAL(ratio),    .description = "Half of the state" },
    { LOCAL(percent),  .description = "The state in percent, rounded" },
    { LOCAL(positive), .description = "True while the state is positive" },
    { STATE(x),        .start = 1.0, .description = "Decaying state" },
};

const modelDefinition MODEL_DEFINITION = {
    .name        = "locals",
    .guid        = "{00000000-0000-0000-0000-000000000003}",
    .description = "Fixture for the test of local variables",

    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),

    .sizeOfOutputs  = sizeof(Outputs),
    .sizeOfLocals   = sizeof(Locals),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrator          = &FORWARD_EULER,
    .maxStepSize         = STEP_SIZE,  /* every step is a single sub-step */
};

/***************************************************
 * FMU helpers
 ***************************************************/

static char lastMessage[512];

static void logger(fmi2ComponentEnvironment environment, fmi2String instanceName, fmi2Status status,
                   fmi2String category, fmi2String message, ...)
{
    (void)environment;
    (void)instanceName;
    (void)status;
    (void)category;

    va_list args;
    va_start(args, message);
    vsnprintf(lastMessage, sizeof(lastMessage), message, args);
    va_end(args);
}

static const fmi2CallbackFunctions CALLBACKS = {
    .logger               = logger,
    .allocateMemory       = calloc,
    .freeMemory           = free,
    .stepFinished         = NULL,
    .componentEnvironment = NULL,
};

static fmi2Component fmu;

/* Value reference of a variable, looked up by name (never hardcoded) */
static fmi2ValueReference vr(const char* name)
{
    for (size_t k = 0; k < MODEL_DEFINITION.numberOfVariables; k++) {
        if (strcmp(MODEL_DEFINITION.variables[k].name, name) == 0) {
            return (fmi2ValueReference)k;
        }
    }
    TEST_FAIL_MESSAGE("unknown variable");
    return 0;
}

static double getReal(const char* name)
{
    fmi2ValueReference r = vr(name);
    fmi2Real value = -1.0;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetReal(fmu, &r, 1, &value));
    return value;
}

static int getInteger(const char* name)
{
    fmi2ValueReference r = vr(name);
    fmi2Integer value = -1;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetInteger(fmu, &r, 1, &value));
    return value;
}

static bool getBoolean(const char* name)
{
    fmi2ValueReference r = vr(name);
    fmi2Boolean value = fmi2False;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetBoolean(fmu, &r, 1, &value));
    return value != fmi2False;
}

/* Instantiated FMU with logging switched on */
static void instantiateFmu(void)
{
    fmu = fmi2Instantiate("locals", fmi2CoSimulation, MODEL_DEFINITION.guid, "", &CALLBACKS, fmi2False, fmi2True);
    TEST_ASSERT_NOT_NULL(fmu);
}

/* Instantiated and initialized FMU, starting at time 0 */
static void initializeFmu(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2False, 0.0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2EnterInitializationMode(fmu));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));
}

/* Communication step number k, from k * STEP_SIZE to (k + 1) * STEP_SIZE */
static fmi2Status step(int k)
{
    return fmi2DoStep(fmu, k * STEP_SIZE, STEP_SIZE, fmi2False);
}

void setUp(void)
{
    poisonRatio = false;
    lastMessage[0] = '\0';
    fmu = NULL;
}

void tearDown(void)
{
    fmi2FreeInstance(fmu);
    fmu = NULL;
}

/***************************************************
 * Tests
 ***************************************************/

void test_locals_ofAllTypes_areComputedAtTheCommunicationPoint(void)
{
    initializeFmu();

    TEST_ASSERT_EQUAL_INT(fmi2OK, step(0));

    /* one Euler step: x = 1 - 0.1 = 0.9 */
    double x = getReal("x");
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.9, x);
    TEST_ASSERT_EQUAL_DOUBLE(x / 2.0, getReal("ratio"));
    TEST_ASSERT_EQUAL_INT(90, getInteger("percent"));
    TEST_ASSERT_TRUE(getBoolean("positive"));
}

void test_locals_followASetWithoutAStep(void)
{
    instantiateFmu();
    fmi2ValueReference x = vr("x");
    fmi2Real value = -4.0;

    /* the state can be set before the simulation; the locals follow at once */
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetReal(fmu, &x, 1, &value));

    TEST_ASSERT_EQUAL_DOUBLE(-2.0, getReal("ratio"));
    TEST_ASSERT_EQUAL_INT(-400, getInteger("percent"));
    TEST_ASSERT_FALSE(getBoolean("positive"));
}

void test_settingARealLocal_isAnError(void)
{
    initializeFmu();
    fmi2ValueReference r = vr("ratio");
    fmi2Real value = 1.0;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2SetReal(fmu, &r, 1, &value));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "'ratio' is a local variable and cannot be set"));

    /* the FMU is in the error state */
    TEST_ASSERT_EQUAL_INT(fmi2Error, step(0));
}

void test_settingAnIntegerLocal_isAnError(void)
{
    initializeFmu();
    fmi2ValueReference r = vr("percent");
    fmi2Integer value = 1;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2SetInteger(fmu, &r, 1, &value));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "'percent' is a local variable and cannot be set"));
}

void test_settingABooleanLocal_isAnError(void)
{
    /* also before the initialization: a local can never be set */
    instantiateFmu();
    fmi2ValueReference r = vr("positive");
    fmi2Boolean value = fmi2False;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2SetBoolean(fmu, &r, 1, &value));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "'positive' is a local variable and cannot be set"));
}

void test_nonFiniteLocal_discardsTheStep(void)
{
    initializeFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, step(0));
    double xBefore = getReal("x");
    double ratioBefore = getReal("ratio");

    poisonRatio = true;
    TEST_ASSERT_EQUAL_INT(fmi2Discard, step(1));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "Local variable 'ratio' is not finite"));

    /* rolled back to the communication point of the discarded step */
    fmi2Real lastSuccessfulTime = -1.0;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetRealStatus(fmu, fmi2LastSuccessfulTime, &lastSuccessfulTime));
    TEST_ASSERT_EQUAL_DOUBLE(STEP_SIZE, lastSuccessfulTime);
    TEST_ASSERT_EQUAL_DOUBLE(xBefore, getReal("x"));
    TEST_ASSERT_EQUAL_DOUBLE(ratioBefore, getReal("ratio"));

    /* the master can only go on from a snapshot, not step again */
    TEST_ASSERT_EQUAL_INT(fmi2Error, step(1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_locals_ofAllTypes_areComputedAtTheCommunicationPoint);
    RUN_TEST(test_locals_followASetWithoutAStep);
    RUN_TEST(test_settingARealLocal_isAnError);
    RUN_TEST(test_settingAnIntegerLocal_isAnError);
    RUN_TEST(test_settingABooleanLocal_isAnError);
    RUN_TEST(test_nonFiniteLocal_discardsTheStep);
    return UNITY_END();
}
