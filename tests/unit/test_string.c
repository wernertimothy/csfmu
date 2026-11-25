/**
 * Test of string parameters in the model layer: the macros recognize a
 * char[MODEL_STRING_SIZE] field, newModel()/resetModel() write the start
 * text, modelSetString() refuses what does not fit (and never cuts it), and
 * the text is part of the model data. No FMI involved.
 */

#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "model.h"

/* Longest text that fits into a string parameter */
#define LONGEST (MODEL_STRING_SIZE - 1)

typedef struct {
    char   file[MODEL_STRING_SIZE];
    double gain;
    char   label[MODEL_STRING_SIZE];
    char   too_long[MODEL_STRING_SIZE];
} Params;

/* A start text that does not fit, filled in by setUp() (the build would refuse it) */
static char tooLongStart[MODEL_STRING_SIZE + 10];

enum { FILE_VAR, GAIN, LABEL, TOO_LONG, NUMBER_OF_VARIABLES };

static const modelVariable VARIABLES[NUMBER_OF_VARIABLES] = {
    [FILE_VAR] = { PARAM(file), .startString = "table.csv", .variability = VAR_FIXED },
    [GAIN]     = { PARAM(gain), .start = 2.0 },
    [LABEL]    = { PARAM(label) },  /* no start text: "" */
    [TOO_LONG] = { PARAM(too_long), .startString = tooLongStart },
};

static const modelDefinition DEFINITION = {
    .name              = "string_model",
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .sizeOfParams      = sizeof(Params),
};

/* A char* field matches char* in _Generic as well, but it has no storage for
 * a text: the macro records its size, so it can be told apart */
typedef struct { char* text; } PointerParams;

static const modelVariable POINTER_VARIABLES[] = {
    { MODEL_VARIABLE(VAR_PARAMETER, PointerParams, text, "text", MODEL_PARAMETER_TYPES), .startString = "x" },
};

static const modelDefinition POINTER_DEFINITION = {
    .name              = "pointer_model",
    .variables         = POINTER_VARIABLES,
    .numberOfVariables = ARRAY_LEN(POINTER_VARIABLES),
    .sizeOfParams      = sizeof(PointerParams),
};

static modelInstance* model;
static Params* p;

/* A text of the given length: "aaa...a" */
static char text[MODEL_STRING_SIZE + 10];
static const char* textOfLength(size_t length)
{
    memset(text, 'a', length);
    text[length] = '\0';
    return text;
}

void setUp(void)
{
    memset(tooLongStart, 'x', sizeof(tooLongStart) - 1);
    tooLongStart[sizeof(tooLongStart) - 1] = '\0';

    model = newModel(&DEFINITION);
    TEST_ASSERT_NOT_NULL(model);
    p = model->params;
}

void tearDown(void)
{
    freeModel(model);
    model = NULL;
}

/***************************************************
 * Declaration
 ***************************************************/

void test_charArrayParameter_isAStringOfTheFieldSize(void)
{
    TEST_ASSERT_EQUAL_INT(VAR_STRING, VARIABLES[FILE_VAR].type);
    TEST_ASSERT_EQUAL_INT(VAR_PARAMETER, VARIABLES[FILE_VAR].role);
    TEST_ASSERT_EQUAL_size_t(MODEL_STRING_SIZE, VARIABLES[FILE_VAR].size);
    TEST_ASSERT_EQUAL_size_t(offsetof(Params, file), VARIABLES[FILE_VAR].offset);
}

void test_otherFields_recordTheirSizeToo(void)
{
    TEST_ASSERT_EQUAL_INT(VAR_REAL, VARIABLES[GAIN].type);
    TEST_ASSERT_EQUAL_size_t(sizeof(double), VARIABLES[GAIN].size);
}

void test_charPointerParameter_isAStringWithThePointerSize(void)
{
    TEST_ASSERT_EQUAL_INT(VAR_STRING, POINTER_VARIABLES[0].type);
    TEST_ASSERT_EQUAL_size_t(sizeof(char*), POINTER_VARIABLES[0].size);
}

/***************************************************
 * Start values
 ***************************************************/

void test_newModel_writesTheStartText(void)
{
    TEST_ASSERT_EQUAL_STRING("table.csv", p->file);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, p->gain);
}

void test_newModel_startsAStringWithoutStartTextEmpty(void)
{
    TEST_ASSERT_EQUAL_STRING("", p->label);
}

void test_newModel_leavesAStringEmptyIfItsStartTextDoesNotFit(void)
{
    TEST_ASSERT_EACH_EQUAL_CHAR('\0', p->too_long, MODEL_STRING_SIZE);
}

void test_resetModel_restoresTheStartTexts(void)
{
    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, "other.csv"));
    TEST_ASSERT_TRUE(modelSetString(model, LABEL, "label"));

    resetModel(model);

    TEST_ASSERT_EQUAL_STRING("table.csv", p->file);
    TEST_ASSERT_EQUAL_STRING("", p->label);
}

/***************************************************
 * modelSetString
 ***************************************************/

void test_modelSetString_acceptsTheLongestText(void)
{
    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, textOfLength(LONGEST)));
    TEST_ASSERT_EQUAL_STRING(text, p->file);
}

void test_modelSetString_refusesATextOneByteTooLong(void)
{
    TEST_ASSERT_FALSE(modelSetString(model, FILE_VAR, textOfLength(LONGEST + 1)));
    TEST_ASSERT_EQUAL_STRING("table.csv", p->file);
}

void test_modelSetString_refusesATextFarTooLong(void)
{
    TEST_ASSERT_FALSE(modelSetString(model, FILE_VAR, tooLongStart));
    TEST_ASSERT_EQUAL_STRING("table.csv", p->file);
}

void test_modelSetString_refusesNull(void)
{
    TEST_ASSERT_FALSE(modelSetString(model, FILE_VAR, NULL));
    TEST_ASSERT_FALSE(modelSetString(NULL, FILE_VAR, "x"));
    TEST_ASSERT_EQUAL_STRING("table.csv", p->file);
}

void test_modelSetString_refusesAVariableThatIsNoString(void)
{
    TEST_ASSERT_FALSE(modelSetString(model, GAIN, "x"));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, p->gain);
}

void test_modelSetString_refusesAnIndexOutOfRange(void)
{
    TEST_ASSERT_FALSE(modelSetString(model, NUMBER_OF_VARIABLES, "x"));
}

void test_modelSetString_acceptsTheEmptyText(void)
{
    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, ""));
    TEST_ASSERT_EQUAL_STRING("", p->file);
}

void test_modelSetString_zeroesTheBytesBehindAShorterText(void)
{
    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, textOfLength(LONGEST)));
    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, "x"));

    TEST_ASSERT_EQUAL_STRING("x", p->file);
    TEST_ASSERT_EACH_EQUAL_CHAR('\0', p->file + 1, MODEL_STRING_SIZE - 1);
}

void test_modelSetString_acceptsItsOwnText(void)
{
    /* e.g. an FMI master that passes back the pointer of fmi2GetString */
    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, p->file));
    TEST_ASSERT_EQUAL_STRING("table.csv", p->file);
}

void test_modelSetString_acceptsAnOverlappingText(void)
{
    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, p->file + 6));
    TEST_ASSERT_EQUAL_STRING("csv", p->file);
}

void test_modelSetString_neverWritesAStringOfAnotherSize(void)
{
    modelInstance* pointerModel = newModel(&POINTER_DEFINITION);
    TEST_ASSERT_NOT_NULL(pointerModel);
    const PointerParams* pp = pointerModel->params;

    /* the start text was not written either: the pointer is still NULL */
    TEST_ASSERT_NULL(pp->text);
    TEST_ASSERT_FALSE(modelSetString(pointerModel, 0, "x"));
    TEST_ASSERT_NULL(pp->text);

    freeModel(pointerModel);
}

/***************************************************
 * Model data
 ***************************************************/

void test_strings_arePartOfTheModelData(void)
{
    void* buffer = malloc(modelDataSize(&DEFINITION));

    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, "a.csv"));
    saveModelData(model, buffer);
    TEST_ASSERT_TRUE(modelSetString(model, FILE_VAR, "b.csv"));

    restoreModelData(model, buffer);
    free(buffer);

    TEST_ASSERT_EQUAL_STRING("a.csv", p->file);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_charArrayParameter_isAStringOfTheFieldSize);
    RUN_TEST(test_otherFields_recordTheirSizeToo);
    RUN_TEST(test_charPointerParameter_isAStringWithThePointerSize);
    RUN_TEST(test_newModel_writesTheStartText);
    RUN_TEST(test_newModel_startsAStringWithoutStartTextEmpty);
    RUN_TEST(test_newModel_leavesAStringEmptyIfItsStartTextDoesNotFit);
    RUN_TEST(test_resetModel_restoresTheStartTexts);
    RUN_TEST(test_modelSetString_acceptsTheLongestText);
    RUN_TEST(test_modelSetString_refusesATextOneByteTooLong);
    RUN_TEST(test_modelSetString_refusesATextFarTooLong);
    RUN_TEST(test_modelSetString_refusesNull);
    RUN_TEST(test_modelSetString_refusesAVariableThatIsNoString);
    RUN_TEST(test_modelSetString_refusesAnIndexOutOfRange);
    RUN_TEST(test_modelSetString_acceptsTheEmptyText);
    RUN_TEST(test_modelSetString_zeroesTheBytesBehindAShorterText);
    RUN_TEST(test_modelSetString_acceptsItsOwnText);
    RUN_TEST(test_modelSetString_acceptsAnOverlappingText);
    RUN_TEST(test_modelSetString_neverWritesAStringOfAnotherSize);
    RUN_TEST(test_strings_arePartOfTheModelData);
    return UNITY_END();
}
