#include "unity.h"
#include "model.h"

void setUp(void) {}
void tearDown(void) {}

void test_freeModel_handles_Null(void) {
    freeModel(NULL);
    TEST_ASSERT_TRUE(1);
}

void test_freeModel_deallocatesMemory(void) {
    modelInstance* model = newModel(2,3,4,5);
    TEST_ASSERT_NOT_NULL(model);
    freeModel(model);
    TEST_ASSERT_TRUE(1);
}

void test_freeModel_worksMultipleTimes(void) {
    modelInstance* model = NULL;
    for (int k=0; k<10; k++) {
        model = newModel(k+1, k+2, k+3, k+4);
        TEST_ASSERT_NOT_NULL(model);
        freeModel(model);
    }
    TEST_ASSERT_TRUE(1);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_freeModel_handles_Null);
    RUN_TEST(test_freeModel_deallocatesMemory);

    return UNITY_END();
}