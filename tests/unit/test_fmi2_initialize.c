/**
 * Test of the initialization through the FMI wrapper: calculated parameters
 * are consistent in every FMU state, the model's initialize function runs
 * once in fmi2ExitInitializationMode, its errors are reported, and a state
 * with initial="approx" can only be set before the initialization.
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

#define STEP_SIZE 0.1

/***************************************************
 * Test model: dx/dt = -twice_gain*x, y = x, twice_gain = 2*gain
 *
 * start_mode 1 lets initialize start the state at twice_gain, start_mode 0
 * keeps its start value.
 ***************************************************/

typedef struct { double gain; double twice_gain; int start_mode; } Params;
typedef struct { double y; int initializations; } Outputs;
typedef struct { double x; } States;
typedef struct { int initializations; } Internals;

/* What initialize does besides its normal work */
typedef enum { INIT_NORMAL, INIT_ERROR, INIT_DISCARD, INIT_NAN } initBehavior;
static initBehavior behavior = INIT_NORMAL;

static void calculateParameters(modelInstance* m)
{
    Params* p = m->params;
    p->twice_gain = 2.0 * p->gain;
}

static simStatus initialize(modelInstance* m)
{
    const Params* p = m->params;
    Internals* internals = m->internals;

    internals->initializations++;

    switch (behavior) {
        case INIT_ERROR:
            return modelError(m, "start_mode %d is not supported", p->start_mode);
        case INIT_DISCARD:
            return SIM_DISCARD;
        case INIT_NAN:
            m->states[0] = NAN;
            return SIM_OK;
        case INIT_NORMAL:
            break;
    }

    if (p->start_mode == 1) {
        m->states[0] = p->twice_gain;
    }
    return SIM_OK;
}

static void stateEquation(modelInstance* m)
{
    const Params* p = m->params;
    m->stateDerivatives[0] = -p->twice_gain * m->states[0];
}

static void outputEquation(modelInstance* m)
{
    Outputs* out = m->outputs;
    out->y = m->states[0];
    out->initializations = ((const Internals*)m->internals)->initializations;
}

/* Explicit Euler */
static void integrator(modelInstance* m, double stepSize)
{
    double t0 = m->time;
    m->states[0] += stepSize * m->stateDerivatives[0];
    m->time = t0 + stepSize;
}

static const modelVariable VARIABLES[] = {
    { PARAM(gain),                 .start = 0.5, .description = "Gain (tunable)" },
    { CALCULATED_PARAM(twice_gain), .description = "Twice the gain" },
    { PARAM(start_mode),           .variability = VAR_FIXED,
      .description = "0: x starts at its start value, 1: x starts at twice_gain" },
    { OUTPUT(y),                   .dependsOn = "", .description = "The state" },
    { OUTPUT(initializations),     .dependsOn = "", .description = "Calls of initialize" },
    { STATE(x),                    .start = 1.0, .initial = VAR_APPROX, .description = "Decaying state" },
};

const modelDefinition MODEL_DEFINITION = {
    .name        = "initialization",
    .guid        = "{00000000-0000-0000-0000-000000000004}",
    .description = "Fixture for the test of the initialization",

    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),

    .sizeOfParams    = sizeof(Params),
    .sizeOfOutputs   = sizeof(Outputs),
    .sizeOfInternals = sizeof(Internals),
    .numberOfStates  = NUMBER_OF_DOUBLES(States),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrate           = integrator,
    .maxStepSize         = STEP_SIZE,  /* every step is a single sub-step */

    .calculateParameters = calculateParameters,
    .initialize          = initialize,
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

static fmi2Status setReal(const char* name, double value)
{
    fmi2ValueReference r = vr(name);
    return fmi2SetReal(fmu, &r, 1, &value);
}

static fmi2Status setInteger(const char* name, int value)
{
    fmi2ValueReference r = vr(name);
    return fmi2SetInteger(fmu, &r, 1, &value);
}

/* Instantiated FMU with logging switched on */
static void instantiateFmu(void)
{
    fmu = fmi2Instantiate("initialization", fmi2CoSimulation, MODEL_DEFINITION.guid, "", &CALLBACKS,
                          fmi2False, fmi2True);
    TEST_ASSERT_NOT_NULL(fmu);
}

/* Set up an instantiated FMU and enter initialization mode */
static void enterInitialization(void)
{
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2False, 0.0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2EnterInitializationMode(fmu));
}

/* Communication step number k, from k * STEP_SIZE to (k + 1) * STEP_SIZE */
static fmi2Status step(int k)
{
    return fmi2DoStep(fmu, k * STEP_SIZE, STEP_SIZE, fmi2False);
}

void setUp(void)
{
    behavior = INIT_NORMAL;
    lastMessage[0] = '\0';
    fmu = NULL;
}

void tearDown(void)
{
    fmi2FreeInstance(fmu);
    fmu = NULL;
}

/***************************************************
 * Calculated parameters
 ***************************************************/

void test_calculatedParameter_followsItsParameterInEveryState(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_DOUBLE(1.0, getReal("twice_gain"));

    /* instantiated */
    TEST_ASSERT_EQUAL_INT(fmi2OK, setReal("gain", 2.0));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, getReal("twice_gain"));

    /* initialization mode */
    enterInitialization();
    TEST_ASSERT_EQUAL_INT(fmi2OK, setReal("gain", 3.0));
    TEST_ASSERT_EQUAL_DOUBLE(6.0, getReal("twice_gain"));

    /* after the initialization */
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));
    TEST_ASSERT_EQUAL_DOUBLE(6.0, getReal("twice_gain"));

    /* between steps (gain is tunable) */
    TEST_ASSERT_EQUAL_INT(fmi2OK, step(0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, setReal("gain", 1.0));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, getReal("twice_gain"));
}

void test_calculatedParameter_reachesTheStateEquationOfTheNextStep(void)
{
    instantiateFmu();
    enterInitialization();
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));

    /* x = 1 - 0.1 * 1 * 1 = 0.9 with twice_gain = 1 */
    TEST_ASSERT_EQUAL_INT(fmi2OK, step(0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.9, getReal("x"));

    /* set gain and step at once, without reading a variable in between:
     * x = 0.9 - 0.1 * 4 * 0.9 = 0.54 with twice_gain = 4 */
    TEST_ASSERT_EQUAL_INT(fmi2OK, setReal("gain", 2.0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, step(1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.54, getReal("x"));
}

void test_settingACalculatedParameter_isAnError(void)
{
    instantiateFmu();

    TEST_ASSERT_EQUAL_INT(fmi2Error, setReal("twice_gain", 3.0));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "'twice_gain' is a calculated parameter and cannot be set"));
}

/***************************************************
 * The model's initialize function
 ***************************************************/

void test_initialize_runsOnceAtTheEndOfTheInitialization(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, setInteger("start_mode", 1));
    enterInitialization();
    TEST_ASSERT_EQUAL_INT(fmi2OK, setReal("gain", 2.0));

    /* not yet: in initialization mode the state still has its start value */
    TEST_ASSERT_EQUAL_DOUBLE(1.0, getReal("y"));
    TEST_ASSERT_EQUAL_INT(0, getInteger("initializations"));

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));

    /* initialize saw the calculated parameter, and the outputs see its result */
    TEST_ASSERT_EQUAL_DOUBLE(4.0, getReal("x"));
    TEST_ASSERT_EQUAL_DOUBLE(4.0, getReal("y"));
    TEST_ASSERT_EQUAL_INT(1, getInteger("initializations"));

    /* steps do not initialize again */
    TEST_ASSERT_EQUAL_INT(fmi2OK, step(0));
    TEST_ASSERT_EQUAL_INT(1, getInteger("initializations"));
}

void test_initializeError_failsWithTheModelsMessage(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, setInteger("start_mode", 7));
    enterInitialization();
    behavior = INIT_ERROR;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2ExitInitializationMode(fmu));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "Initialization failed: start_mode 7 is not supported"));

    /* the FMU is in the error state */
    TEST_ASSERT_EQUAL_INT(fmi2Error, step(0));
}

void test_initializeDiscard_isAnError(void)
{
    /* fmi2ExitInitializationMode cannot be repeated, so there is nothing to recover */
    instantiateFmu();
    enterInitialization();
    behavior = INIT_DISCARD;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2ExitInitializationMode(fmu));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "Initialization failed"));
}

void test_nonFiniteStateAfterInitialize_isAnError(void)
{
    instantiateFmu();
    enterInitialization();
    behavior = INIT_NAN;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2ExitInitializationMode(fmu));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "state 'x' is not finite"));
}

void test_reset_initializesAgain(void)
{
    instantiateFmu();
    enterInitialization();
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));
    TEST_ASSERT_EQUAL_INT(1, getInteger("initializations"));

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2Reset(fmu));
    TEST_ASSERT_EQUAL_INT(fmi2OK, setInteger("start_mode", 1));
    TEST_ASSERT_EQUAL_INT(fmi2OK, setReal("gain", 2.5));
    enterInitialization();
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));

    /* the reset zeroed the internals, and the new initialization ran */
    TEST_ASSERT_EQUAL_INT(1, getInteger("initializations"));
    TEST_ASSERT_EQUAL_DOUBLE(5.0, getReal("x"));
}

/***************************************************
 * States with initial="approx"
 ***************************************************/

void test_approxState_canBeSetWhenInstantiated(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, setReal("x", 2.0));

    enterInitialization();
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));

    /* start_mode 0: the initialization keeps the start value */
    TEST_ASSERT_EQUAL_DOUBLE(2.0, getReal("x"));
}

void test_approxState_cannotBeSetInInitializationMode(void)
{
    instantiateFmu();
    enterInitialization();

    TEST_ASSERT_EQUAL_INT(fmi2Error, setReal("x", 2.0));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "'x' has initial=\"approx\""));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_calculatedParameter_followsItsParameterInEveryState);
    RUN_TEST(test_calculatedParameter_reachesTheStateEquationOfTheNextStep);
    RUN_TEST(test_settingACalculatedParameter_isAnError);
    RUN_TEST(test_initialize_runsOnceAtTheEndOfTheInitialization);
    RUN_TEST(test_initializeError_failsWithTheModelsMessage);
    RUN_TEST(test_initializeDiscard_isAnError);
    RUN_TEST(test_nonFiniteStateAfterInitialize_isAnError);
    RUN_TEST(test_reset_initializesAgain);
    RUN_TEST(test_approxState_canBeSetWhenInstantiated);
    RUN_TEST(test_approxState_cannotBeSetInInitializationMode);
    return UNITY_END();
}
