/**
 * C-level test of the discard and rollback path of fmi2DoStep, with the
 * fault_injection model. It calls the fmi2 functions directly (no FMPy), so
 * that the sanitizer job (ASan/UBSan) covers the rollback memory copies.
 *
 * The Python tests in test_discard.py cover the same protocol through FMPy.
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

static fmi2Status setFail(bool fail)
{
    fmi2ValueReference r = vr("fail");
    fmi2Boolean value = fail ? fmi2True : fmi2False;
    return fmi2SetBoolean(fmu, &r, 1, &value);
}

/* Initialized FMU with the fault switched on, stepped to t = 0.5 */
void setUp(void)
{
    fmu = fmi2Instantiate("discard", fmi2CoSimulation, MODEL_DEFINITION.guid, "", &CALLBACKS, fmi2False, fmi2True);
    TEST_ASSERT_NOT_NULL(fmu);
    TEST_ASSERT_EQUAL_INT(fmi2OK, setFail(true));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2False, 0.0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2EnterInitializationMode(fmu));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));

    for (int k = 0; k < 5; k++) {
        TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DoStep(fmu, k * STEP_SIZE, STEP_SIZE, fmi2False));
    }
}

void tearDown(void)
{
    fmi2FreeInstance(fmu);
}

void test_discard_rollsBackBitExactly(void)
{
    double x = getReal("x");
    double y = getReal("y");

    TEST_ASSERT_EQUAL_INT(fmi2Discard, fmi2DoStep(fmu, 0.5, STEP_SIZE, fmi2False));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "rolled back to 0.5"));

    /* bit-identical, not only close */
    double xAfter = getReal("x");
    double yAfter = getReal("y");
    TEST_ASSERT_EQUAL_MEMORY(&x, &xAfter, sizeof(double));
    TEST_ASSERT_EQUAL_MEMORY(&y, &yAfter, sizeof(double));
}

void test_discard_reportsTheStatus(void)
{
    TEST_ASSERT_EQUAL_INT(fmi2Discard, fmi2DoStep(fmu, 0.5, STEP_SIZE, fmi2False));

    fmi2Status doStepStatus = fmi2OK;
    fmi2Real lastSuccessfulTime = -1.0;
    fmi2Boolean terminated = fmi2True;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetStatus(fmu, fmi2DoStepStatus, &doStepStatus));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetRealStatus(fmu, fmi2LastSuccessfulTime, &lastSuccessfulTime));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetBooleanStatus(fmu, fmi2Terminated, &terminated));

    TEST_ASSERT_EQUAL_INT(fmi2Discard, doStepStatus);
    TEST_ASSERT_EQUAL_DOUBLE(0.5, lastSuccessfulTime);
    TEST_ASSERT_EQUAL_INT(fmi2False, terminated);
}

void test_discard_recoversThroughASnapshot(void)
{
    fmi2FMUstate atStart = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetFMUstate(fmu, &atStart));
    TEST_ASSERT_EQUAL_INT(fmi2Discard, fmi2DoStep(fmu, 0.5, STEP_SIZE, fmi2False));

    /* Step Failed: no step and no set until the state is restored */
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetFMUstate(fmu, atStart));
    TEST_ASSERT_EQUAL_INT(fmi2OK, setFail(false));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DoStep(fmu, 0.5, STEP_SIZE, fmi2False));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, exp(-0.6), getReal("x"));

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &atStart));
}

void test_discard_snapshotOfTheFailedStateCanBeSerialized(void)
{
    TEST_ASSERT_EQUAL_INT(fmi2Discard, fmi2DoStep(fmu, 0.5, STEP_SIZE, fmi2False));

    fmi2FMUstate failed = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetFMUstate(fmu, &failed));

    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SerializedFMUstateSize(fmu, failed, &size));
    fmi2Byte* bytes = malloc(size);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SerializeFMUstate(fmu, failed, bytes, size));

    fmi2FMUstate restored = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DeSerializeFMUstate(fmu, bytes, size, &restored));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetFMUstate(fmu, restored));

    fmi2Status doStepStatus = fmi2OK;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetStatus(fmu, fmi2DoStepStatus, &doStepStatus));
    TEST_ASSERT_EQUAL_INT(fmi2Discard, doStepStatus);

    free(bytes);
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &failed));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &restored));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_discard_rollsBackBitExactly);
    RUN_TEST(test_discard_reportsTheStatus);
    RUN_TEST(test_discard_recoversThroughASnapshot);
    RUN_TEST(test_discard_snapshotOfTheFailedStateCanBeSerialized);
    return UNITY_END();
}
