#include "unity.h"
#include "model.h"

typedef struct { double a; int b; } Params;
typedef struct { double u; } Inputs;
typedef struct { double y; } Outputs;

static const modelDefinition DEFINITION = {
    .sizeOfParams   = sizeof(Params),
    .sizeOfInputs   = sizeof(Inputs),
    .sizeOfOutputs  = sizeof(Outputs),
    .numberOfStates = 2,
};

void setUp(void) {}
void tearDown(void) {}

void test_freeModel_handles_Null(void) {
    freeModel(NULL);
    TEST_ASSERT_TRUE(1);
}

void test_freeModel_deallocatesMemory(void) {
    modelInstance* model = newModel(&DEFINITION);
    TEST_ASSERT_NOT_NULL(model);
    freeModel(model);
    TEST_ASSERT_TRUE(1);
}

void test_freeModel_worksMultipleTimes(void) {
    modelInstance* model = NULL;
    for (int k=0; k<10; k++) {
        model = newModel(&DEFINITION);
        TEST_ASSERT_NOT_NULL(model);
        freeModel(model);
    }
    TEST_ASSERT_TRUE(1);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_freeModel_handles_Null);
    RUN_TEST(test_freeModel_deallocatesMemory);
    RUN_TEST(test_freeModel_worksMultipleTimes);

    return UNITY_END();
}
