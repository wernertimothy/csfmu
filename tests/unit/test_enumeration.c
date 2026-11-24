/**
 * Test of enumeration types in the model layer: modelEnumerationHasValue()
 * and enumeration variables, which stay Integer variables. No FMI involved.
 */

#include "unity.h"
#include "model.h"

typedef struct { int mode; } Params;

/* values with a gap (2) and a negative value */
static const modelEnumerationItem ITEMS[] = {
    { .name = "reverse", .value = -1 },
    { .name = "slow",    .value = 1 },
    { .name = "fast",    .value = 3 },
};

static const modelEnumeration SPEED = {
    .name          = "Speed",
    .items         = ITEMS,
    .numberOfItems = ARRAY_LEN(ITEMS),
};

static const modelEnumeration EMPTY = { .name = "Empty" };

static const modelVariable VARIABLES[] = {
    { PARAM(mode), .declaredType = &SPEED, .start = 3 },
};

static const modelDefinition DEFINITION = {
    .name              = "enumeration_model",
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .sizeOfParams      = sizeof(Params),
};

void setUp(void) {}
void tearDown(void) {}

void test_modelEnumerationHasValue_acceptsTheItemValues(void)
{
    TEST_ASSERT_TRUE(modelEnumerationHasValue(&SPEED, -1));
    TEST_ASSERT_TRUE(modelEnumerationHasValue(&SPEED, 1));
    TEST_ASSERT_TRUE(modelEnumerationHasValue(&SPEED, 3));
}

void test_modelEnumerationHasValue_rejectsAValueBetweenItems(void)
{
    TEST_ASSERT_FALSE(modelEnumerationHasValue(&SPEED, 2));
    TEST_ASSERT_FALSE(modelEnumerationHasValue(&SPEED, 0));
}

void test_modelEnumerationHasValue_rejectsValuesOutsideTheItems(void)
{
    TEST_ASSERT_FALSE(modelEnumerationHasValue(&SPEED, -2));
    TEST_ASSERT_FALSE(modelEnumerationHasValue(&SPEED, 4));
}

void test_modelEnumerationHasValue_handlesNullAndEmptyTypes(void)
{
    TEST_ASSERT_FALSE(modelEnumerationHasValue(NULL, 1));
    TEST_ASSERT_FALSE(modelEnumerationHasValue(&EMPTY, 0));
}

void test_enumerationVariable_isAnIntegerVariable(void)
{
    TEST_ASSERT_EQUAL_INT(VAR_INTEGER, VARIABLES[0].type);
    TEST_ASSERT_EQUAL_INT(VAR_PARAMETER, VARIABLES[0].role);
    TEST_ASSERT_EQUAL_PTR(&SPEED, VARIABLES[0].declaredType);
}

void test_enumerationVariable_startsAtItsStartValue(void)
{
    modelInstance* model = newModel(&DEFINITION);
    const Params* p = model->params;

    TEST_ASSERT_EQUAL_INT(3, p->mode);

    freeModel(model);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_modelEnumerationHasValue_acceptsTheItemValues);
    RUN_TEST(test_modelEnumerationHasValue_rejectsAValueBetweenItems);
    RUN_TEST(test_modelEnumerationHasValue_rejectsValuesOutsideTheItems);
    RUN_TEST(test_modelEnumerationHasValue_handlesNullAndEmptyTypes);
    RUN_TEST(test_enumerationVariable_isAnIntegerVariable);
    RUN_TEST(test_enumerationVariable_startsAtItsStartValue);
    return UNITY_END();
}
