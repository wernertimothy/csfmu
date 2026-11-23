/**
 * Test of the internals block (modelDefinition.sizeOfInternals) through the
 * FMI wrapper: the internals are part of the FMU state, of the serialized FMU
 * state and of the rollback of a discarded step, and fmi2Reset zeroes them.
 *
 * The FMI wrapper (src/fmi2Functions.c) is linked against the model
 * definition below instead of a real model, as in
 * test_fmi2_integratorContract.c, so the test is built for every model and
 * runs in the sanitizer job.
 *
 * The internals are not published, so the test reads them through the model
 * instance the fixture's functions were last called with (fixtureModel).
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "fmi2Functions.h"
#include "model.h"

/* Communication step size, and the number of sub-steps fmi2DoStep divides it into */
#define STEP_SIZE 0.1
#define SUBSTEPS  2

/***************************************************
 * Test model: dx/dt = -x, y = x, steps = number of integrator sub-steps
 ***************************************************/

typedef struct { double y; int steps; } Outputs;
typedef struct { double x; } States;
typedef struct { int steps; } Internals;

/* The integrator fails (NaN state) when it reaches this sub-step, 0 = never */
static int failAtStep = 0;

/* Model instance the fixture's functions were last called with */
static modelInstance* fixtureModel = NULL;

static void stateEquation(modelInstance* m)
{
    fixtureModel = m;
    m->stateDerivatives[0] = -m->states[0];
}

/* The output equation only reads the internals */
static void outputEquation(modelInstance* m)
{
    const Internals* internals = m->internals;
    Outputs* out = m->outputs;

    fixtureModel = m;
    out->y = m->states[0];
    out->steps = internals->steps;
}

/* Explicit Euler that counts its sub-steps in the internals. The test
 * integrator writes the internals because the hooks that write them in a
 * real model (initialization, discrete and event updates) do not exist yet;
 * a model's own state and output equations only read them */
static void integrator(modelInstance* m, double stepSize)
{
    Internals* internals = m->internals;
    double t0 = m->time;

    fixtureModel = m;
    internals->steps++;
    m->states[0] += stepSize * m->stateDerivatives[0];
    if (internals->steps == failAtStep) {
        m->states[0] = NAN;
    }
    m->time = t0 + stepSize;
}

static const modelVariable VARIABLES[] = {
    { OUTPUT(y),     .dependsOn = "", .description = "The state" },
    { OUTPUT(steps), .dependsOn = "", .description = "Number of integrator sub-steps, copied from the internals" },
    { STATE(x),      .start = 1.0,    .description = "Decaying state" },
};

const modelDefinition MODEL_DEFINITION = {
    .name        = "internals",
    .guid        = "{00000000-0000-0000-0000-000000000002}",
    .description = "Fixture for the test of the internals block",

    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),

    .sizeOfOutputs   = sizeof(Outputs),
    .sizeOfInternals = sizeof(Internals),
    .numberOfStates  = NUMBER_OF_DOUBLES(States),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrate           = integrator,
    .maxStepSize         = STEP_SIZE / SUBSTEPS,
};

/***************************************************
 * FMU helpers
 ***************************************************/

static void logger(fmi2ComponentEnvironment environment, fmi2String instanceName, fmi2Status status,
                   fmi2String category, fmi2String message, ...)
{
    (void)environment;
    (void)instanceName;
    (void)status;
    (void)category;
    (void)message;
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
    fmi2Integer value = -1;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetInteger(fmu, &r, 1, &value));
    return value;
}

/* Value of the internals of the FMU's model instance */
static int internalSteps(void)
{
    TEST_ASSERT_NOT_NULL(fixtureModel);
    return ((const Internals*)fixtureModel->internals)->steps;
}

/* Set up and initialize an instantiated FMU, starting at time 0 */
static void initializeExperiment(void)
{
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2False, 0.0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2EnterInitializationMode(fmu));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));
}

/* Instantiated and initialized FMU, starting at time 0 */
static void initializeFmu(void)
{
    fmu = fmi2Instantiate("internals", fmi2CoSimulation, MODEL_DEFINITION.guid, "", &CALLBACKS, fmi2False, fmi2False);
    TEST_ASSERT_NOT_NULL(fmu);
    initializeExperiment();
}

/* Communication step number k, from k * STEP_SIZE to (k + 1) * STEP_SIZE */
static fmi2Status step(int k)
{
    return fmi2DoStep(fmu, k * STEP_SIZE, STEP_SIZE, fmi2False);
}

/* Communication steps first, ..., last, each of which must succeed */
static void steps(int first, int last)
{
    for (int k = first; k <= last; k++) {
        TEST_ASSERT_EQUAL_INT(fmi2OK, step(k));
    }
}

void setUp(void)
{
    failAtStep = 0;
    fixtureModel = NULL;
    fmu = NULL;
}

void tearDown(void)
{
    fmi2FreeInstance(fmu);
}

/***************************************************
 * Tests
 ***************************************************/

void test_internals_areZeroAfterInitialization(void)
{
    initializeFmu();

    TEST_ASSERT_EQUAL_INT(0, internalSteps());
    TEST_ASSERT_EQUAL_INT(0, getInteger("steps"));
}

void test_internals_keepTheirValueFromStepToStep(void)
{
    initializeFmu();

    steps(0, 2);

    TEST_ASSERT_EQUAL_INT(3 * SUBSTEPS, internalSteps());
    TEST_ASSERT_EQUAL_INT(3 * SUBSTEPS, getInteger("steps"));
}

void test_setFMUstate_restoresTheInternals(void)
{
    initializeFmu();
    steps(0, 1);

    fmi2FMUstate state = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetFMUstate(fmu, &state));
    steps(2, 3);
    int first = getInteger("steps");

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetFMUstate(fmu, state));
    TEST_ASSERT_EQUAL_INT(2 * SUBSTEPS, internalSteps());

    /* Stepping again repeats the trajectory, also in the output computed from the internals */
    steps(2, 3);
    TEST_ASSERT_EQUAL_INT(first, getInteger("steps"));

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &state));
}

void test_serializedFMUstate_carriesTheInternals(void)
{
    initializeFmu();
    steps(0, 2);

    /* Serialize a snapshot taken after three steps */
    fmi2FMUstate state = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetFMUstate(fmu, &state));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SerializedFMUstateSize(fmu, state, &size));
    fmi2Byte* bytes = malloc(size);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SerializeFMUstate(fmu, state, bytes, size));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &state));

    /* Start over: the internals are zero again */
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2Reset(fmu));
    initializeExperiment();
    TEST_ASSERT_EQUAL_INT(0, internalSteps());

    /* The deserialized snapshot brings the internals back */
    fmi2FMUstate restored = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DeSerializeFMUstate(fmu, bytes, size, &restored));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetFMUstate(fmu, restored));
    TEST_ASSERT_EQUAL_INT(3 * SUBSTEPS, internalSteps());

    /* ... and the simulation continues from them */
    steps(3, 3);
    TEST_ASSERT_EQUAL_INT(4 * SUBSTEPS, getInteger("steps"));

    free(bytes);
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &restored));
}

void test_reset_zeroesTheInternals(void)
{
    initializeFmu();
    steps(0, 1);

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2Reset(fmu));
    TEST_ASSERT_EQUAL_INT(0, internalSteps());

    initializeExperiment();
    TEST_ASSERT_EQUAL_INT(0, getInteger("steps"));
}

/* The last sub-step of the third communication step fails. Its earlier
 * sub-steps have already advanced the internals, and the rollback to the
 * communication point takes them back as well */
void test_discardedStep_rollsBackTheInternals(void)
{
    initializeFmu();
    steps(0, 1);
    failAtStep = 3 * SUBSTEPS;

    TEST_ASSERT_EQUAL_INT(fmi2Discard, step(2));

    TEST_ASSERT_EQUAL_INT(2 * SUBSTEPS, internalSteps());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_internals_areZeroAfterInitialization);
    RUN_TEST(test_internals_keepTheirValueFromStepToStep);
    RUN_TEST(test_setFMUstate_restoresTheInternals);
    RUN_TEST(test_serializedFMUstate_carriesTheInternals);
    RUN_TEST(test_reset_zeroesTheInternals);
    RUN_TEST(test_discardedStep_rollsBackTheInternals);
    return UNITY_END();
}
