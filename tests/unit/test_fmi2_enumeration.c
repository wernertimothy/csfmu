/**
 * Test of enumeration variables through the FMI wrapper: they are set and
 * read with fmi2Set/GetInteger, and only the values of their items can be
 * set (also not a value between two items).
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
 * Test model: dx/dt = -level*x, y = x, echo = command
 ***************************************************/

typedef struct { int level; } Params;
typedef struct { int command; } Inputs;
typedef struct { double y; int echo; } Outputs;
typedef struct { double x; } States;

/* Items with a gap: 2 is no item */
static const modelEnumerationItem LEVEL_ITEMS[] = {
    { .name = "low",  .value = 1 },
    { .name = "high", .value = 3 },
};
static const modelEnumeration LEVEL = {
    .name = "Level", .items = LEVEL_ITEMS, .numberOfItems = ARRAY_LEN(LEVEL_ITEMS),
};

static const modelEnumerationItem DIRECTION_ITEMS[] = {
    { .name = "backward", .value = -1 },
    { .name = "forward",  .value = 1 },
};
static const modelEnumeration DIRECTION = {
    .name = "Direction", .items = DIRECTION_ITEMS, .numberOfItems = ARRAY_LEN(DIRECTION_ITEMS),
};

static void stateEquation(modelInstance* m)
{
    const Params* p = m->params;
    m->stateDerivatives[0] = -p->level * m->states[0];
}

static void outputEquation(modelInstance* m)
{
    const Inputs* in = m->inputs;
    Outputs* out = m->outputs;
    out->y = m->states[0];
    out->echo = in->command;
}

static const modelVariable VARIABLES[] = {
    { PARAM(level),    .declaredType = &LEVEL, .start = 1, .description = "Decay level" },
    { INPUT(command),  .declaredType = &DIRECTION, .start = 1, .description = "Commanded direction" },
    { OUTPUT(y),       .dependsOn = "", .description = "The state" },
    { OUTPUT(echo),    .declaredType = &DIRECTION, .dependsOn = "command", .description = "The command" },
    { STATE(x),        .start = 1.0, .description = "Decaying state" },
};

const modelDefinition MODEL_DEFINITION = {
    .name        = "enumeration",
    .guid        = "{00000000-0000-0000-0000-000000000005}",
    .description = "Fixture for the test of enumeration variables",

    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),

    .sizeOfParams   = sizeof(Params),
    .sizeOfInputs   = sizeof(Inputs),
    .sizeOfOutputs  = sizeof(Outputs),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrator          = &FORWARD_EULER,
    .maxStepSize         = STEP_SIZE,
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

static int getInteger(const char* name)
{
    fmi2ValueReference r = vr(name);
    fmi2Integer value = -99;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetInteger(fmu, &r, 1, &value));
    return value;
}

static fmi2Status setInteger(const char* name, int value)
{
    fmi2ValueReference r = vr(name);
    return fmi2SetInteger(fmu, &r, 1, &value);
}

/* Instantiated FMU with logging switched on */
static void instantiateFmu(void)
{
    fmu = fmi2Instantiate("enumeration", fmi2CoSimulation, MODEL_DEFINITION.guid, "", &CALLBACKS,
                          fmi2False, fmi2True);
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

void setUp(void)
{
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

void test_itemValues_canBeSetAndRead(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(1, getInteger("level"));
    TEST_ASSERT_EQUAL_INT(1, getInteger("command"));

    TEST_ASSERT_EQUAL_INT(fmi2OK, setInteger("level", 3));
    TEST_ASSERT_EQUAL_INT(fmi2OK, setInteger("command", -1));

    TEST_ASSERT_EQUAL_INT(3, getInteger("level"));
    TEST_ASSERT_EQUAL_INT(-1, getInteger("command"));
    TEST_ASSERT_EQUAL_INT(-1, getInteger("echo"));  /* computed by the model */
}

void test_valueBetweenItems_isRefused(void)
{
    instantiateFmu();

    TEST_ASSERT_EQUAL_INT(fmi2Error, setInteger("level", 2));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "Value 2 of variable 'level' is not an item of the enumeration 'Level'"));

    /* the variable keeps its value (variables can be read in the error state) */
    TEST_ASSERT_EQUAL_INT(1, getInteger("level"));
}

void test_valueBelowTheItems_isRefused(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2Error, setInteger("level", 0));
}

void test_valueAboveTheItems_isRefused(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2Error, setInteger("level", 4));
}

void test_inputs_areCheckedBetweenSteps(void)
{
    initializeFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DoStep(fmu, 0.0, STEP_SIZE, fmi2False));

    TEST_ASSERT_EQUAL_INT(fmi2OK, setInteger("command", -1));
    TEST_ASSERT_EQUAL_INT(fmi2Error, setInteger("command", 0));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "Value 0 of variable 'command' is not an item of the enumeration 'Direction'"));
}

void test_enumeration_cannotBeSetAsReal(void)
{
    instantiateFmu();
    fmi2ValueReference r = vr("level");
    fmi2Real value = 3.0;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2SetReal(fmu, &r, 1, &value));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_itemValues_canBeSetAndRead);
    RUN_TEST(test_valueBetweenItems_isRefused);
    RUN_TEST(test_valueBelowTheItems_isRefused);
    RUN_TEST(test_valueAboveTheItems_isRefused);
    RUN_TEST(test_inputs_areCheckedBetweenSteps);
    RUN_TEST(test_enumeration_cannotBeSetAsReal);
    return UNITY_END();
}
