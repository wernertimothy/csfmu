/**
 * Tests of simulateTo() (src/simulation.c), the FMI-agnostic orchestrator,
 * and of the model-layer helpers it reports with.
 *
 * A model definition of the test records every evaluation of the state
 * equation, every integration step and every evaluation of the output
 * equation, so the tests can check where the steps are, when f is
 * evaluated, where the clock ends, and what a failure leaves behind.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "model.h"
#include "simulation.h"
#include "integrator.h"

/***************************************************
 * Test model: dx/dt = -x, y = 2*x, z = x + 1
 ***************************************************/

typedef struct { double y; } Outputs;
typedef struct { double z; } Locals;
typedef struct { double x; } States;

#define MAX_RECORDS 16

/* What the test model records */
static int derivativeCalls;
static double derivativeTimes[MAX_RECORDS];
static int integratorCalls;
static double integratorTimes[MAX_RECORDS];
static double integratorSteps[MAX_RECORDS];
static int outputCalls;
static double outputTime;

/* Faults the test can switch on */
static int nanDerivativeAtCall;  /* the state equation returns NaN at this call (-1: never) */
static bool nanOutput;
static bool nanLocal;
static bool forgetTime;          /* the integrator does not advance the time */
static double stepFraction;      /* the integrator reports a step of stepFraction * hMax (1: correct) */
static simStatus stepResult;     /* status the integrator returns after advancing the state */

static void stateEquation(modelInstance* m)
{
    if (derivativeCalls < MAX_RECORDS) {
        derivativeTimes[derivativeCalls] = m->time;
    }
    m->stateDerivatives[0] = (derivativeCalls == nanDerivativeAtCall) ? NAN : -m->states[0];
    derivativeCalls++;
}

static void outputEquation(modelInstance* m)
{
    outputCalls++;
    outputTime = m->time;
    ((Outputs*)m->outputs)->y = nanOutput ? NAN : 2.0 * m->states[0];
    ((Locals*)m->locals)->z = nanLocal ? NAN : m->states[0] + 1.0;
}

/* Explicit Euler that records its calls */
static simStatus recordingStep(modelInstance* m, double hMax, double* hTaken)
{
    if (integratorCalls < MAX_RECORDS) {
        integratorTimes[integratorCalls] = m->time;
        integratorSteps[integratorCalls] = hMax;
    }
    integratorCalls++;

    double t0 = m->time;
    m->states[0] += hMax * m->stateDerivatives[0];
    if (!forgetTime) {
        m->time = t0 + hMax;
    }
    *hTaken = stepFraction * hMax;
    return stepResult;
}

static const integratorDef RECORDING_EULER = {
    .name  = "recording Euler",
    .step  = recordingStep,
    .order = 1,
};

/* Explicit Euler that uses all of its work memory: 3 vectors of length nx */
static simStatus scratchStep(modelInstance* m, double hMax, double* hTaken)
{
    size_t nx = m->def->numberOfStates;
    for (size_t i = 0; i < 3 * nx; i++) {
        m->scratch[i] = (double)i;
    }
    return EULER_FORWARD.step(m, hMax, hTaken);
}

static const integratorDef SCRATCH_3 = {
    .name           = "Euler with work memory",
    .step           = scratchStep,
    .scratchVectors = 3,
    .order          = 1,
};

static const modelVariable VARIABLES[] = {
    { OUTPUT(y), .dependsOn = "" },
    { LOCAL(z) },
    { STATE(x),  .start = 1.0 },
};

static const modelDefinition DEFINITION = {
    .name              = "simulation",
    .guid              = "{00000000-0000-0000-0000-000000000010}",
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .sizeOfOutputs     = sizeof(Outputs),
    .sizeOfLocals      = sizeof(Locals),
    .numberOfStates    = NUMBER_OF_DOUBLES(States),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrator          = &RECORDING_EULER,
    .maxStepSize         = 0.1,
};

static modelInstance* model;
static void* before;
static void* after;

/* Whether the model data equal those saved in `before`, bit for bit */
static bool modelDataUnchanged(void)
{
    saveModelData(model, after);
    return memcmp(before, after, modelDataSize(model->def)) == 0;
}

void setUp(void)
{
    derivativeCalls = integratorCalls = outputCalls = 0;
    outputTime = NAN;
    nanDerivativeAtCall = -1;
    nanOutput = nanLocal = forgetTime = false;
    stepFraction = 1.0;
    stepResult = SIM_OK;

    model = newModel(&DEFINITION);
    before = malloc(modelDataSize(&DEFINITION));
    after = malloc(modelDataSize(&DEFINITION));
}

void tearDown(void)
{
    freeModel(model);
    free(before);
    free(after);
}

/***************************************************
 * Steps and clock
 ***************************************************/

void test_interval_isCutIntoEqualSteps(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(model, 0.3));

    /* 0.3 / 0.1 is 2.9999999999999996 in floating point: still 3 steps */
    TEST_ASSERT_EQUAL_INT(3, integratorCalls);
    double h = 0.3 / 3.0;
    for (int k = 0; k < 3; k++) {
        TEST_ASSERT_EQUAL_DOUBLE(h, integratorSteps[k]);
        /* each step starts at t + k*h, from the integer k */
        TEST_ASSERT_EQUAL_DOUBLE(0.0 + (double)k * h, integratorTimes[k]);
    }
}

void test_derivatives_areEvaluatedAtTheStartOfEveryStep(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(model, 0.3));

    TEST_ASSERT_EQUAL_INT(3, derivativeCalls);
    for (int k = 0; k < 3; k++) {
        TEST_ASSERT_EQUAL_DOUBLE(integratorTimes[k], derivativeTimes[k]);
    }
}

void test_clockEndsExactlyAtTheEnd_andOutputsAreEvaluatedThere(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(model, 0.3));

    TEST_ASSERT_TRUE(model->time == 0.3);
    TEST_ASSERT_EQUAL_INT(1, outputCalls);
    TEST_ASSERT_TRUE(outputTime == 0.3);
    TEST_ASSERT_EQUAL_DOUBLE(2.0 * model->states[0], ((Outputs*)model->outputs)->y);
    TEST_ASSERT_EQUAL_DOUBLE(model->states[0] + 1.0, ((Locals*)model->locals)->z);
}

void test_intervalShorterThanMaxStepSize_isOneStep(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(model, 0.05));

    TEST_ASSERT_EQUAL_INT(1, integratorCalls);
    TEST_ASSERT_EQUAL_DOUBLE(0.05, integratorSteps[0]);
}

/* 3 * 0.1 is 0.30000000000000004 and 0.4 - 0.30000000000000004 is
 * 0.09999999999999998: the step integrates exactly the interval the clock
 * advances, not the nominal 0.1 */
void test_stepSize_isTheLengthOfTheInterval(void)
{
    model->time = 3.0 * 0.1;
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(model, 0.4));

    TEST_ASSERT_EQUAL_INT(1, integratorCalls);
    TEST_ASSERT_TRUE(integratorSteps[0] == 0.4 - 3.0 * 0.1);
    TEST_ASSERT_TRUE(model->time == 0.4);
}

/* A step so short that t + h rounds to t: nothing to integrate, but the
 * outputs are evaluated like after every step */
void test_emptyInterval_takesNoStep_butEvaluatesTheOutputs(void)
{
    model->time = 1.0e6;
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(model, 1.0e6 + 1.0e-12));

    TEST_ASSERT_EQUAL_INT(0, integratorCalls);
    TEST_ASSERT_EQUAL_INT(0, derivativeCalls);
    TEST_ASSERT_EQUAL_INT(1, outputCalls);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, model->states[0]);
    TEST_ASSERT_TRUE(model->time == 1.0e6);
}

/***************************************************
 * Refused calls: SIM_ERROR, nothing changed
 ***************************************************/

void test_endBeforeTheStart_isRefused(void)
{
    model->time = 1.0;
    saveModelData(model, before);

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, 0.5));
    TEST_ASSERT_NOT_NULL(strstr(model->message, "Cannot simulate from t = 1 to t = 0.5"));
    TEST_ASSERT_EQUAL_INT(0, integratorCalls);
    TEST_ASSERT_TRUE(modelDataUnchanged());
}

void test_endThatIsNotFinite_isRefused(void)
{
    saveModelData(model, before);

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, NAN));
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, INFINITY));
    TEST_ASSERT_EQUAL_INT(0, integratorCalls);
    TEST_ASSERT_TRUE(modelDataUnchanged());
}

void test_tooManySteps_isRefused(void)
{
    saveModelData(model, before);

    /* 1e9 / 0.1 = 1e10 steps */
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, 1.0e9));
    TEST_ASSERT_NOT_NULL(strstr(model->message, "needs more than 1000000000 integration steps"));
    TEST_ASSERT_EQUAL_INT(0, integratorCalls);
    TEST_ASSERT_TRUE(modelDataUnchanged());
}

void test_maxStepSizeThatIsNotPositive_isRefused(void)
{
    static modelDefinition invalid;  /* static: the model keeps a pointer to it until tearDown() */
    invalid = DEFINITION;
    invalid.maxStepSize = 0.0;
    freeModel(model);
    model = newModel(&invalid);

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, 0.1));
    TEST_ASSERT_NOT_NULL(strstr(model->message, "maxStepSize must be greater than 0"));
    TEST_ASSERT_EQUAL_INT(0, integratorCalls);
}

/***************************************************
 * Numerical failure: SIM_DISCARD, rolled back to the start
 ***************************************************/

void test_nonFiniteState_isDiscarded_andRestoresAllModelData(void)
{
    ((Outputs*)model->outputs)->y = 42.0;  /* the outputs of the start are restored, too */
    saveModelData(model, before);
    nanDerivativeAtCall = 1;  /* the second of three steps fails */

    TEST_ASSERT_EQUAL_INT(SIM_DISCARD, simulateTo(model, 0.3));
    TEST_ASSERT_EQUAL_STRING("State 'x' is not finite at t = 0.2.", model->message);
    TEST_ASSERT_EQUAL_INT(2, integratorCalls);  /* not continued after the failure */

    /* back at the start (not at the last good step), bit for bit */
    TEST_ASSERT_TRUE(modelDataUnchanged());
    TEST_ASSERT_TRUE(model->time == 0.0);
}

void test_nonFiniteOutput_isDiscarded(void)
{
    saveModelData(model, before);
    nanOutput = true;

    TEST_ASSERT_EQUAL_INT(SIM_DISCARD, simulateTo(model, 0.3));
    TEST_ASSERT_EQUAL_STRING("Output 'y' is not finite at t = 0.3.", model->message);
    TEST_ASSERT_TRUE(modelDataUnchanged());
}

void test_nonFiniteLocal_isDiscarded(void)
{
    saveModelData(model, before);
    nanLocal = true;

    TEST_ASSERT_EQUAL_INT(SIM_DISCARD, simulateTo(model, 0.3));
    TEST_ASSERT_EQUAL_STRING("Local variable 'z' is not finite at t = 0.3.", model->message);
    TEST_ASSERT_TRUE(modelDataUnchanged());
}

void test_successAfterAFailure_clearsTheMessage(void)
{
    nanOutput = true;
    TEST_ASSERT_EQUAL_INT(SIM_DISCARD, simulateTo(model, 0.1));

    nanOutput = false;
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(model, 0.1));
    TEST_ASSERT_EQUAL_STRING("", model->message);
}

/***************************************************
 * Broken integrator: SIM_ERROR, values kept for diagnosis
 ***************************************************/

void test_brokenIntegratorContract_isAnError_withoutRollback(void)
{
    forgetTime = true;

    /* a single step of exactly 0.1, so the message shows the expected end as 0.1 */
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, 0.1));
    TEST_ASSERT_NOT_NULL(strstr(model->message, "left model->time at 0 instead of 0.1"));

    /* the state keeps what the integrator computed */
    TEST_ASSERT_EQUAL_INT(1, integratorCalls);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.9, model->states[0]);
}

/***************************************************
 * The integrator descriptor
 ***************************************************/

void test_modelWithoutIntegrator_isRefused(void)
{
    saveModelData(model, before);
    model->integrator = NULL;

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, 0.1));
    TEST_ASSERT_NOT_NULL(strstr(model->message, "has no integrator"));
    TEST_ASSERT_TRUE(modelDataUnchanged());
}

/* A fixed-step method must take the whole step it is given */
void test_stepOfAnotherLength_isAnError(void)
{
    stepFraction = 0.5;

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, 0.1));
    TEST_ASSERT_NOT_NULL(strstr(model->message, "The integrator 'recording Euler' took a step of 0.05 instead of 0.1"));
}

void test_stepThatDiscards_restoresAllModelData(void)
{
    saveModelData(model, before);
    stepResult = SIM_DISCARD;

    TEST_ASSERT_EQUAL_INT(SIM_DISCARD, simulateTo(model, 0.3));
    TEST_ASSERT_EQUAL_INT(1, integratorCalls);  /* stops at the first failed step */
    TEST_ASSERT_TRUE(modelDataUnchanged());
    /* the integrator gave no reason, so simulateTo() names it */
    TEST_ASSERT_EQUAL_STRING("The integrator 'recording Euler' could not take the step from t = 0.", model->message);
}

void test_stepThatFails_isAnError_withoutRollback(void)
{
    stepResult = SIM_ERROR;

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, simulateTo(model, 0.1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.9, model->states[0]);  /* kept for diagnosis */
}

void test_workMemory_isSizedForTheIntegrator(void)
{
    static modelDefinition withScratch;  /* static: the model keeps a pointer to it until tearDown() */
    withScratch = DEFINITION;
    withScratch.integrator = &SCRATCH_3;
    freeModel(model);
    model = newModel(&withScratch);

    TEST_ASSERT_NOT_NULL(model->scratch);
    /* the step writes all 3 * nx doubles: AddressSanitizer checks the size */
    TEST_ASSERT_EQUAL_INT(SIM_OK, simulateTo(model, 0.1));
}

/***************************************************
 * Model-layer helpers
 ***************************************************/

void test_modelDiscard_setsTheMessage(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_DISCARD, modelDiscard(model, "step %d of %s", 3, "x"));
    TEST_ASSERT_EQUAL_STRING("step 3 of x", model->message);
}

void test_modelStateName_namesTheState(void)
{
    TEST_ASSERT_EQUAL_STRING("x", modelStateName(&DEFINITION, 0));
    TEST_ASSERT_EQUAL_STRING("?", modelStateName(&DEFINITION, 1));
}

void test_firstNonFinite_findsNothingInAFiniteModel(void)
{
    TEST_ASSERT_EQUAL_size_t(DEFINITION.numberOfStates, modelFirstNonFiniteState(model));
    TEST_ASSERT_EQUAL_size_t(DEFINITION.numberOfVariables, modelFirstNonFiniteResult(model));
}

void test_timeRoundingTolerance_scalesWithTheTime(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(8.0 * 2.220446049250313e-16, timeRoundingTolerance(0.5));
    TEST_ASSERT_EQUAL_DOUBLE(8.0 * 2.220446049250313e-16 * 1000.0, timeRoundingTolerance(-1000.0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_interval_isCutIntoEqualSteps);
    RUN_TEST(test_derivatives_areEvaluatedAtTheStartOfEveryStep);
    RUN_TEST(test_clockEndsExactlyAtTheEnd_andOutputsAreEvaluatedThere);
    RUN_TEST(test_intervalShorterThanMaxStepSize_isOneStep);
    RUN_TEST(test_stepSize_isTheLengthOfTheInterval);
    RUN_TEST(test_emptyInterval_takesNoStep_butEvaluatesTheOutputs);
    RUN_TEST(test_endBeforeTheStart_isRefused);
    RUN_TEST(test_endThatIsNotFinite_isRefused);
    RUN_TEST(test_tooManySteps_isRefused);
    RUN_TEST(test_maxStepSizeThatIsNotPositive_isRefused);
    RUN_TEST(test_nonFiniteState_isDiscarded_andRestoresAllModelData);
    RUN_TEST(test_nonFiniteOutput_isDiscarded);
    RUN_TEST(test_nonFiniteLocal_isDiscarded);
    RUN_TEST(test_successAfterAFailure_clearsTheMessage);
    RUN_TEST(test_brokenIntegratorContract_isAnError_withoutRollback);
    RUN_TEST(test_modelWithoutIntegrator_isRefused);
    RUN_TEST(test_stepOfAnotherLength_isAnError);
    RUN_TEST(test_stepThatDiscards_restoresAllModelData);
    RUN_TEST(test_stepThatFails_isAnError_withoutRollback);
    RUN_TEST(test_workMemory_isSizedForTheIntegrator);
    RUN_TEST(test_modelDiscard_setsTheMessage);
    RUN_TEST(test_modelStateName_namesTheState);
    RUN_TEST(test_firstNonFinite_findsNothingInAFiniteModel);
    RUN_TEST(test_timeRoundingTolerance_scalesWithTheTime);
    return UNITY_END();
}
