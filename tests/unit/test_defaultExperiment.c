/**
 * Test of the DefaultExperiment element of the modelDescription.xml
 * generator (writeDefaultExperiment in src/fmi2ModelDescription.c).
 *
 * The generator is included as source, so that its static functions can be
 * called; its main() is renamed. The golden test (fixtures/generator_model.c)
 * covers an experiment with all four settings. This test covers experiments
 * that suggest only some of them, or none.
 */

#define main generator_main
#include "../../src/fmi2ModelDescription.c"
#undef main

#include <string.h>
#include "unity.h"

/* Written into the working directory of the test (its build folder), not
   with tmpfile(), which may need rights in the root of the drive on Windows */
#define OUTPUT_FILE "test_defaultExperiment.xml"

/* generator_main() refers to MODEL_DEFINITION; it is never called here */
const modelDefinition MODEL_DEFINITION = { .name = "default_experiment_model" };

static char written[512];

/* Writes the DefaultExperiment element of experiment and returns the text */
static const char* writeExperiment(modelExperiment experiment)
{
    /* binary mode: no line-ending translation on Windows */
    FILE* file = fopen(OUTPUT_FILE, "w+b");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, "cannot open " OUTPUT_FILE);

    writeDefaultExperiment(file, &experiment);

    rewind(file);
    size_t length = fread(written, 1, sizeof(written) - 1, file);
    written[length] = '\0';
    fclose(file);
    remove(OUTPUT_FILE);
    return written;
}

void setUp(void) {}
void tearDown(void) {}

void test_nothingSuggested_writesNoElement(void)
{
    TEST_ASSERT_EQUAL_STRING("", writeExperiment((modelExperiment){ 0 }));
}

void test_toleranceOnly_isPublished(void)
{
    TEST_ASSERT_EQUAL_STRING("    <DefaultExperiment tolerance=\"1e-08\"/>\n\n",
                             writeExperiment((modelExperiment){ .tolerance = 1e-8 }));
}

void test_stepSizeOnly_isPublished(void)
{
    TEST_ASSERT_EQUAL_STRING("    <DefaultExperiment stepSize=\"0.01\"/>\n\n",
                             writeExperiment((modelExperiment){ .stepSize = 0.01 }));
}

void test_timeWindowOnly_isPublished(void)
{
    TEST_ASSERT_EQUAL_STRING("    <DefaultExperiment startTime=\"0\" stopTime=\"5\"/>\n\n",
                             writeExperiment((modelExperiment){ .stopTime = 5.0 }));
}

void test_allSettings_arePublished(void)
{
    modelExperiment experiment = { .startTime = 0.5, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.1 };
    TEST_ASSERT_EQUAL_STRING(
        "    <DefaultExperiment startTime=\"0.5\" stopTime=\"10\" tolerance=\"1e-06\" stepSize=\"0.1\"/>\n\n",
        writeExperiment(experiment));
}

/* A stop time that does not lie behind the start time is no time window,
   so neither time is written; the tolerance still is */
void test_stopTimeNotBehindStartTime_writesNoTimeWindow(void)
{
    modelExperiment experiment = { .startTime = 2.0, .stopTime = 2.0, .tolerance = 1e-6 };
    TEST_ASSERT_EQUAL_STRING("    <DefaultExperiment tolerance=\"1e-06\"/>\n\n", writeExperiment(experiment));
}

/* A tolerance or step size that is not greater than 0 is not set */
void test_nonPositiveToleranceAndStepSize_areNotPublished(void)
{
    modelExperiment experiment = { .stopTime = 1.0, .tolerance = -1e-6, .stepSize = -0.1 };
    TEST_ASSERT_EQUAL_STRING("    <DefaultExperiment startTime=\"0\" stopTime=\"1\"/>\n\n", writeExperiment(experiment));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nothingSuggested_writesNoElement);
    RUN_TEST(test_toleranceOnly_isPublished);
    RUN_TEST(test_stepSizeOnly_isPublished);
    RUN_TEST(test_timeWindowOnly_isPublished);
    RUN_TEST(test_allSettings_arePublished);
    RUN_TEST(test_stopTimeNotBehindStartTime_writesNoTimeWindow);
    RUN_TEST(test_nonPositiveToleranceAndStepSize_areNotPublished);
    return UNITY_END();
}
