/**
 * Test of the integrator time contract (integrateFn in include/model.h):
 * on return model->time is t + stepSize. fmi2DoStep checks this after every
 * sub-step and fails with fmi2Error otherwise.
 *
 * The FMI wrapper (src/fmi2Functions.c) is linked against the model
 * definition below instead of a real model. Its integrator can be told to
 * break the contract, so the test needs no broken model in src/models/.
 *
 * The same fixture covers calls that FMPy cannot make, such as
 * fmi2Instantiate without callbacks.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "fmi2Functions.h"
#include "model.h"

/***************************************************
 * Test model: dx/dt = -x, y = 2*x
 ***************************************************/

typedef struct { double y; } Outputs;
typedef struct { double x; } States;

/* How the test integrator handles the time */
typedef enum {
    TIME_CORRECT,     /* t + h, as the contract requires */
    TIME_FORGOTTEN,   /* the time is not advanced */
    TIME_OFF_BY_ULPS  /* t + h, moved by `ulps` units in the last place */
} timeBehavior;

static timeBehavior behavior = TIME_CORRECT;
static int ulps = 0;

static void stateEquation(modelInstance* m)
{
    m->stateDerivatives[0] = -m->states[0];
}

static void outputEquation(modelInstance* m)
{
    ((Outputs*)m->outputs)->y = 2.0 * m->states[0];
}

/* Explicit Euler whose handling of the time is chosen by the test */
static void integrator(modelInstance* m, double stepSize)
{
    double t0 = m->time;
    m->states[0] += stepSize * m->stateDerivatives[0];

    switch (behavior) {
        case TIME_CORRECT:
            m->time = t0 + stepSize;
            break;
        case TIME_FORGOTTEN:
            break;
        case TIME_OFF_BY_ULPS:
            m->time = t0 + stepSize;
            for (int k = 0; k < ulps; k++) {
                m->time = nextafter(m->time, INFINITY);
            }
            break;
    }
}

static const modelVariable VARIABLES[] = {
    { OUTPUT(y), .dependsOn = "", .description = "Twice the state" },
    { STATE(x),  .start = 1.0,    .description = "Decaying state" },
};

const modelDefinition MODEL_DEFINITION = {
    .name        = "integrator_contract",
    .guid        = "{00000000-0000-0000-0000-000000000001}",
    .description = "Fixture for the test of the integrator time contract",

    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),

    .sizeOfOutputs  = sizeof(Outputs),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrate           = integrator,
    .maxStepSize         = 1.0,  /* every step of this test is a single sub-step */
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
    double value = NAN;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetReal(fmu, &r, 1, &value));
    return value;
}

/* Set up and initialize an instantiated FMU, starting at startTime */
static void initializeExperiment(double startTime)
{
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetupExperiment(fmu, fmi2False, 0.0, startTime, fmi2False, 0.0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2EnterInitializationMode(fmu));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));
}

/* Instantiated and initialized FMU, starting at startTime */
static void initializeFmu(double startTime)
{
    fmu = fmi2Instantiate("contract", fmi2CoSimulation, MODEL_DEFINITION.guid, "", &CALLBACKS, fmi2False, fmi2True);
    TEST_ASSERT_NOT_NULL(fmu);
    initializeExperiment(startTime);
}

void setUp(void)
{
    behavior = TIME_CORRECT;
    ulps = 0;
    fmu = NULL;
}

void tearDown(void)
{
    fmi2FreeInstance(fmu);
}

/***************************************************
 * Tests
 ***************************************************/

void test_correctIntegrator_stepsNormally(void)
{
    initializeFmu(0.0);

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DoStep(fmu, 0.0, 0.1, fmi2False));

    fmi2Real time = -1.0;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetRealStatus(fmu, fmi2LastSuccessfulTime, &time));
    TEST_ASSERT_EQUAL_DOUBLE(0.1, time);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.9, getReal("x"));
}

void test_forgottenTime_failsWithError(void)
{
    initializeFmu(0.0);
    behavior = TIME_FORGOTTEN;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2DoStep(fmu, 0.0, 0.1, fmi2False));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "left model->time at 0 instead of 0.1"));

    /* the error state refuses further steps ... */
    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2DoStep(fmu, 0.0, 0.1, fmi2False));

    /* ... until the FMU is reset */
    behavior = TIME_CORRECT;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2Reset(fmu));
    initializeExperiment(0.0);
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DoStep(fmu, 0.0, 0.1, fmi2False));
}

void test_errorState_outputsMatchTheVariables(void)
{
    initializeFmu(0.0);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, getReal("y"));  /* outputs are up to date at t = 0 */

    behavior = TIME_FORGOTTEN;
    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2DoStep(fmu, 0.0, 0.1, fmi2False));

    /* the model keeps what the integrator computed, and the outputs follow it */
    double x = getReal("x");
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.9, x);
    TEST_ASSERT_EQUAL_DOUBLE(2.0 * x, getReal("y"));
}

/* At t = 1e6 one ulp (1.16e-10) is far more than 1e-6 * h for h = 1e-6, so a
 * purely relative check would reject an integrator that is off by rounding
 * only. The rounding allowance (8 * DBL_EPSILON * t = 1.78e-9) is about 15 ulp */
void test_timeOffByRounding_isAccepted(void)
{
    initializeFmu(1.0e6);
    behavior = TIME_OFF_BY_ULPS;
    ulps = 4;

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DoStep(fmu, 1.0e6, 1.0e-6, fmi2False));
}

void test_timeOffBeyondRounding_failsWithError(void)
{
    initializeFmu(1.0e6);
    behavior = TIME_OFF_BY_ULPS;
    ulps = 100;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2DoStep(fmu, 1.0e6, 1.0e-6, fmi2False));
}

/***************************************************
 * Instantiation without callbacks
 ***************************************************/

/* Without a logger the FMU cannot report anything, so it does not start */
void test_instantiate_withoutCallbacks_returnsNull(void)
{
    TEST_ASSERT_NULL(fmi2Instantiate("contract", fmi2CoSimulation, MODEL_DEFINITION.guid, "", NULL,
                                     fmi2False, fmi2False));
}

void test_instantiate_withoutLogger_returnsNull(void)
{
    const fmi2CallbackFunctions noLogger = {
        .logger         = NULL,
        .allocateMemory = calloc,
        .freeMemory     = free,
    };

    TEST_ASSERT_NULL(fmi2Instantiate("contract", fmi2CoSimulation, MODEL_DEFINITION.guid, "", &noLogger,
                                     fmi2False, fmi2False));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_correctIntegrator_stepsNormally);
    RUN_TEST(test_forgottenTime_failsWithError);
    RUN_TEST(test_errorState_outputsMatchTheVariables);
    RUN_TEST(test_timeOffByRounding_isAccepted);
    RUN_TEST(test_timeOffBeyondRounding_failsWithError);
    RUN_TEST(test_instantiate_withoutCallbacks_returnsNull);
    RUN_TEST(test_instantiate_withoutLogger_returnsNull);
    return UNITY_END();
}
