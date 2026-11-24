/**
 * Test of string parameters through the FMI wrapper: fmi2GetString returns
 * the texts, fmi2SetString copies them in and refuses a text that does not
 * fit (never cutting it), fixed and tunable strings follow the rules of
 * parameters, and the texts are part of the FMU state.
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

/* Longest text that fits into a string parameter */
#define LONGEST (MODEL_STRING_SIZE - 1)

/***************************************************
 * Test model: dx/dt = -gain*x, y = x, label_length = strlen(label)
 ***************************************************/

typedef struct { char file[MODEL_STRING_SIZE]; char label[MODEL_STRING_SIZE]; double gain; } Params;
typedef struct { double y; int label_length; } Outputs;
typedef struct { double x; } States;

static void stateEquation(modelInstance* m)
{
    const Params* p = m->params;
    m->stateDerivatives[0] = -p->gain * m->states[0];
}

/* The output equation reads a string: its result shows that the equations
 * see a text as soon as it is set */
static void outputEquation(modelInstance* m)
{
    const Params* p = m->params;
    Outputs* out = m->outputs;
    out->y = m->states[0];
    out->label_length = (int)strlen(p->label);
}

/* Explicit Euler */
static void integrator(modelInstance* m, double stepSize)
{
    double t0 = m->time;
    m->states[0] += stepSize * m->stateDerivatives[0];
    m->time = t0 + stepSize;
}

static const modelVariable VARIABLES[] = {
    { PARAM(file),  .startString = "table.csv", .variability = VAR_FIXED, .description = "A file name" },
    { PARAM(label), .startString = "abc", .description = "A tunable text" },
    { PARAM(gain),  .start = 1.0, .description = "Decay rate" },
    { OUTPUT(y),    .dependsOn = "", .description = "The state" },
    { OUTPUT(label_length), .dependsOn = "", .description = "Number of bytes of label" },
    { STATE(x),     .start = 1.0, .description = "Decaying state" },
};

const modelDefinition MODEL_DEFINITION = {
    .name        = "string",
    .guid        = "{00000000-0000-0000-0000-000000000006}",
    .description = "Fixture for the test of string parameters",

    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),

    .sizeOfParams   = sizeof(Params),
    .sizeOfOutputs  = sizeof(Outputs),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrate           = integrator,
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

static fmi2String getString(const char* name)
{
    fmi2ValueReference r = vr(name);
    fmi2String value = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetString(fmu, &r, 1, &value));
    TEST_ASSERT_NOT_NULL(value);
    return value;
}

static fmi2Status setString(const char* name, fmi2String value)
{
    fmi2ValueReference r = vr(name);
    return fmi2SetString(fmu, &r, 1, &value);
}

/* A text of the given length: "aaa...a" */
static char text[MODEL_STRING_SIZE + 10];
static const char* textOfLength(size_t length)
{
    memset(text, 'a', length);
    text[length] = '\0';
    return text;
}

/* Instantiated FMU with logging switched on */
static void instantiateFmu(void)
{
    fmu = fmi2Instantiate("string", fmi2CoSimulation, MODEL_DEFINITION.guid, "", &CALLBACKS,
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

void test_startTexts_canBeRead(void)
{
    instantiateFmu();
    const fmi2ValueReference refs[] = { vr("file"), vr("label") };
    fmi2String values[2] = { NULL, NULL };

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetString(fmu, refs, 2, values));

    TEST_ASSERT_EQUAL_STRING("table.csv", values[0]);
    TEST_ASSERT_EQUAL_STRING("abc", values[1]);
}

void test_texts_canBeSetAndRead(void)
{
    instantiateFmu();
    const fmi2ValueReference refs[] = { vr("label"), vr("file") };
    const fmi2String values[] = { "xyz", "other.csv" };

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetString(fmu, refs, 2, values));

    TEST_ASSERT_EQUAL_STRING("other.csv", getString("file"));
    TEST_ASSERT_EQUAL_STRING("xyz", getString("label"));
}

void test_textsAreCopied(void)
{
    instantiateFmu();
    char buffer[] = "copied.csv";

    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("file", buffer));
    buffer[0] = 'X';  /* the integration tool may reuse its memory after the call */

    TEST_ASSERT_EQUAL_STRING("copied.csv", getString("file"));
}

void test_longestText_isAccepted(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("label", textOfLength(LONGEST)));
    TEST_ASSERT_EQUAL_STRING(text, getString("label"));
}

void test_textThatDoesNotFit_isRefused(void)
{
    instantiateFmu();

    TEST_ASSERT_EQUAL_INT(fmi2Error, setString("label", textOfLength(LONGEST + 1)));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "The value of variable 'label' does not fit"));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "at most 255 bytes"));

    /* the variable keeps its value, it is not cut (variables can be read in the error state) */
    TEST_ASSERT_EQUAL_STRING("abc", getString("label"));
}

void test_nullText_isRefused(void)
{
    instantiateFmu();

    TEST_ASSERT_EQUAL_INT(fmi2Error, setString("label", NULL));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "Variable 'label': the value is NULL"));
    TEST_ASSERT_EQUAL_STRING("abc", getString("label"));
}

void test_fixedString_canOnlyBeSetBeforeTheSimulation(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2False, 0.0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2EnterInitializationMode(fmu));
    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("file", "init.csv"));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(fmu));

    TEST_ASSERT_EQUAL_INT(fmi2Error, setString("file", "late.csv"));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "Variable 'file' can only be set before the simulation starts"));
    TEST_ASSERT_EQUAL_STRING("init.csv", getString("file"));
}

void test_tunableString_canBeSetBetweenSteps_andTheOutputsFollow(void)
{
    initializeFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DoStep(fmu, 0.0, STEP_SIZE, fmi2False));

    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("label", "hello"));

    /* the output equation sees the new text without a step */
    fmi2ValueReference r = vr("label_length");
    fmi2Integer length = -1;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetInteger(fmu, &r, 1, &length));
    TEST_ASSERT_EQUAL_INT(5, length);

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DoStep(fmu, STEP_SIZE, STEP_SIZE, fmi2False));
    TEST_ASSERT_EQUAL_STRING("hello", getString("label"));
}

void test_returnedPointer_staysTheSame_andCanBePassedBack(void)
{
    instantiateFmu();
    fmi2String before = getString("label");

    /* passing back the FMU's own text (source and destination overlap) */
    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("label", before));
    TEST_ASSERT_EQUAL_STRING("abc", getString("label"));

    /* nothing is allocated per call: the pointer is the same, its text changes */
    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("label", "other"));
    TEST_ASSERT_EQUAL_PTR(before, getString("label"));
    TEST_ASSERT_EQUAL_STRING("other", before);
}

void test_fmuState_restoresTheTexts(void)
{
    instantiateFmu();
    fmi2FMUstate state = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetFMUstate(fmu, &state));

    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("label", textOfLength(LONGEST)));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetFMUstate(fmu, state));

    TEST_ASSERT_EQUAL_STRING("abc", getString("label"));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &state));
}

void test_serializedFmuState_restoresTheTexts(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("label", "saved"));

    fmi2FMUstate state = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetFMUstate(fmu, &state));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SerializedFMUstateSize(fmu, state, &size));
    fmi2Byte* bytes = malloc(size);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SerializeFMUstate(fmu, state, bytes, size));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &state));

    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("label", "changed"));

    fmi2FMUstate restored = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DeSerializeFMUstate(fmu, bytes, size, &restored));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetFMUstate(fmu, restored));
    TEST_ASSERT_EQUAL_STRING("saved", getString("label"));

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &restored));
    free(bytes);
}

void test_reset_restoresTheStartTexts(void)
{
    initializeFmu();
    TEST_ASSERT_EQUAL_INT(fmi2OK, setString("label", "changed"));

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2Reset(fmu));

    TEST_ASSERT_EQUAL_STRING("abc", getString("label"));
    TEST_ASSERT_EQUAL_STRING("table.csv", getString("file"));
}

void test_string_cannotBeAccessedAsReal(void)
{
    instantiateFmu();
    fmi2ValueReference r = vr("label");
    fmi2Real value = 0.0;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2GetReal(fmu, &r, 1, &value));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "is of type String, not Real"));
}

void test_real_cannotBeAccessedAsString(void)
{
    instantiateFmu();
    TEST_ASSERT_EQUAL_INT(fmi2Error, setString("gain", "1.0"));
    TEST_ASSERT_NOT_NULL(strstr(lastMessage, "is of type Real, not String"));
}

void test_output_cannotBeReadAsString(void)
{
    instantiateFmu();
    fmi2ValueReference r = vr("y");
    fmi2String value = NULL;

    TEST_ASSERT_EQUAL_INT(fmi2Error, fmi2GetString(fmu, &r, 1, &value));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_startTexts_canBeRead);
    RUN_TEST(test_texts_canBeSetAndRead);
    RUN_TEST(test_textsAreCopied);
    RUN_TEST(test_longestText_isAccepted);
    RUN_TEST(test_textThatDoesNotFit_isRefused);
    RUN_TEST(test_nullText_isRefused);
    RUN_TEST(test_fixedString_canOnlyBeSetBeforeTheSimulation);
    RUN_TEST(test_tunableString_canBeSetBetweenSteps_andTheOutputsFollow);
    RUN_TEST(test_returnedPointer_staysTheSame_andCanBePassedBack);
    RUN_TEST(test_fmuState_restoresTheTexts);
    RUN_TEST(test_serializedFmuState_restoresTheTexts);
    RUN_TEST(test_reset_restoresTheStartTexts);
    RUN_TEST(test_string_cannotBeAccessedAsReal);
    RUN_TEST(test_real_cannotBeAccessedAsString);
    RUN_TEST(test_output_cannotBeReadAsString);
    return UNITY_END();
}
