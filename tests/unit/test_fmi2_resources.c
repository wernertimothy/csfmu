/**
 * Test of the resource folder through the FMI wrapper: fmi2Instantiate turns
 * fmuResourceLocation (a file URI, FMI 2.0 section 2.1.5) into the plain path
 * model->resourcePath, ending with '/'. A location that is no file URI
 * leaves the model without resource folder, but never makes the
 * instantiation fail.
 *
 * The test is black-box: the model's initialize function records the path
 * it sees. The FMI wrapper (src/fmi2Functions.c) is linked against the model
 * definition below instead of a real model, as in
 * test_fmi2_integratorContract.c, so the test is built for every model and
 * runs in the sanitizer job.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "fmi2Functions.h"
#include "model.h"

/***************************************************
 * Test model: no states, initialize records the resource path
 ***************************************************/

typedef struct { double gain; } Params;

/* The resource path the model saw in its last initialization ("(none)" for NULL) */
static char seenPath[512];

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }

static void integrator(modelInstance* m, double stepSize)
{
    m->time += stepSize;
}

static simStatus initialize(modelInstance* m)
{
    snprintf(seenPath, sizeof(seenPath), "%s", m->resourcePath ? m->resourcePath : "(none)");
    return SIM_OK;
}

static const modelVariable VARIABLES[] = {
    { PARAM(gain), .start = 1.0 },
};

const modelDefinition MODEL_DEFINITION = {
    .name        = "resources",
    .guid        = "{00000000-0000-0000-0000-000000000007}",
    .description = "Fixture for the test of the resource folder",

    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .sizeOfParams      = sizeof(Params),

    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrate           = integrator,
    .maxStepSize         = 0.1,
    .initialize          = initialize,
};

/***************************************************
 * FMU helpers
 ***************************************************/

static int numberOfWarnings;
static char lastWarning[512];

static void logger(fmi2ComponentEnvironment environment, fmi2String instanceName, fmi2Status status,
                   fmi2String category, fmi2String message, ...)
{
    (void)environment;
    (void)instanceName;
    (void)category;

    if (status == fmi2Warning) {
        numberOfWarnings++;
        va_list args;
        va_start(args, message);
        vsnprintf(lastWarning, sizeof(lastWarning), message, args);
        va_end(args);
    }
}

static const fmi2CallbackFunctions CALLBACKS = {
    .logger               = logger,
    .allocateMemory       = calloc,
    .freeMemory           = free,
    .stepFinished         = NULL,
    .componentEnvironment = NULL,
};

static fmi2Component fmu;
static fmi2Component other;

static fmi2Component instantiate(const char* name, fmi2String location)
{
    fmi2Component c = fmi2Instantiate(name, fmi2CoSimulation, MODEL_DEFINITION.guid, location, &CALLBACKS,
                                      fmi2False, fmi2True);
    TEST_ASSERT_NOT_NULL_MESSAGE(c, "the instantiation must never fail because of the resource location");
    return c;
}

static void initialize_(fmi2Component c)
{
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetupExperiment(c, fmi2False, 0.0, 0.0, fmi2False, 0.0));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2EnterInitializationMode(c));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2ExitInitializationMode(c));
}

/* The resource path the model sees for the given location */
static const char* pathFor(fmi2String location)
{
    fmu = instantiate("resources", location);
    initialize_(fmu);
    return seenPath;
}

void setUp(void)
{
    seenPath[0] = '\0';
    numberOfWarnings = 0;
    lastWarning[0] = '\0';
    fmu = NULL;
    other = NULL;
}

void tearDown(void)
{
    fmi2FreeInstance(fmu);
    fmi2FreeInstance(other);
    fmu = NULL;
    other = NULL;
}

/***************************************************
 * File URIs that are understood
 ***************************************************/

void test_fileUriWithEmptyAuthority(void)
{
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", pathFor("file:///tmp/fmu/resources"));
    TEST_ASSERT_EQUAL_INT(0, numberOfWarnings);
}

void test_fileUriWithTrailingSlash_getsNoSecondOne(void)
{
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", pathFor("file:///tmp/fmu/resources/"));
}

void test_fileUriWithoutAuthority(void)
{
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", pathFor("file:/tmp/fmu/resources"));
}

void test_fileUriWithLocalhost(void)
{
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", pathFor("file://localhost/tmp/fmu/resources"));
}

void test_schemeAndLocalhost_ignoreCase(void)
{
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", pathFor("FILE://LocalHost/tmp/fmu/resources"));
}

void test_windowsDrive_losesTheLeadingSlash(void)
{
    /* both examples of the standard (FMI 2.0, section 2.1.5) */
    TEST_ASSERT_EQUAL_STRING("C:/temp/MyFMU/resources/", pathFor("file:///C:/temp/MyFMU/resources"));
    fmi2FreeInstance(fmu);
    TEST_ASSERT_EQUAL_STRING("C:/temp/MyFMU/resources/", pathFor("file:/C:/temp/MyFMU/resources"));
}

void test_percentEncoding_isDecoded(void)
{
    /* a space, and a-umlaut in UTF-8 with upper and lower case hex digits */
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu with space \xc3\xa4\xc3\xa4/resources/",
                             pathFor("file:///tmp/fmu%20with%20space%20%C3%A4%c3%a4/resources"));
}

/***************************************************
 * Locations without resource folder
 ***************************************************/

void test_nullLocation_givesNoResourceFolder_silently(void)
{
    TEST_ASSERT_EQUAL_STRING("(none)", pathFor(NULL));
    TEST_ASSERT_EQUAL_INT(0, numberOfWarnings);
}

void test_emptyLocation_givesNoResourceFolder_silently(void)
{
    TEST_ASSERT_EQUAL_STRING("(none)", pathFor(""));
    TEST_ASSERT_EQUAL_INT(0, numberOfWarnings);
}

/* A location that is no file URI with an absolute path: no folder, one warning */
static void assertRefused(fmi2String location)
{
    numberOfWarnings = 0;
    TEST_ASSERT_EQUAL_STRING_MESSAGE("(none)", pathFor(location), location);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, numberOfWarnings, location);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(lastWarning, "is not a file URI with an absolute path"), location);
    fmi2FreeInstance(fmu);
    fmu = NULL;
}

void test_otherSchemes_areRefusedWithAWarning(void)
{
    assertRefused("http://example.com/resources");
    assertRefused("/tmp/fmu/resources");  /* a path, not a URI */
}

void test_otherHost_isRefusedWithAWarning(void)
{
    assertRefused("file://server/share/resources");
}

void test_relativePath_isRefusedWithAWarning(void)
{
    assertRefused("file:relative/resources");
}

void test_malformedPercentEncoding_isRefusedWithAWarning(void)
{
    assertRefused("file:///tmp/fmu%2");
    assertRefused("file:///tmp/fmu%zz/resources");
    assertRefused("file:///tmp/fmu%00/resources");
}

/***************************************************
 * The resource folder belongs to the instance
 ***************************************************/

void test_reset_keepsTheResourceFolder(void)
{
    pathFor("file:///tmp/fmu/resources");
    seenPath[0] = '\0';

    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2Reset(fmu));
    initialize_(fmu);

    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", seenPath);
}

void test_everyInstance_hasItsOwnResourceFolder(void)
{
    fmu = instantiate("first", "file:///tmp/first/resources");
    other = instantiate("second", "file:///tmp/second/resources");

    initialize_(fmu);
    TEST_ASSERT_EQUAL_STRING("/tmp/first/resources/", seenPath);
    initialize_(other);
    TEST_ASSERT_EQUAL_STRING("/tmp/second/resources/", seenPath);
}

void test_fmuState_doesNotCarryTheResourceFolder(void)
{
    /* the folder is where this instance was unzipped, not part of the simulation */
    fmu = instantiate("first", "file:///tmp/first/resources");
    other = instantiate("second", "file:///tmp/second/resources");

    fmi2FMUstate state = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2GetFMUstate(fmu, &state));
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SerializedFMUstateSize(fmu, state, &size));
    fmi2Byte* bytes = malloc(size);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SerializeFMUstate(fmu, state, bytes, size));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(fmu, &state));

    fmi2FMUstate restored = NULL;
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2DeSerializeFMUstate(other, bytes, size, &restored));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2SetFMUstate(other, restored));
    TEST_ASSERT_EQUAL_INT(fmi2OK, fmi2FreeFMUstate(other, &restored));
    free(bytes);

    initialize_(other);
    TEST_ASSERT_EQUAL_STRING("/tmp/second/resources/", seenPath);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fileUriWithEmptyAuthority);
    RUN_TEST(test_fileUriWithTrailingSlash_getsNoSecondOne);
    RUN_TEST(test_fileUriWithoutAuthority);
    RUN_TEST(test_fileUriWithLocalhost);
    RUN_TEST(test_schemeAndLocalhost_ignoreCase);
    RUN_TEST(test_windowsDrive_losesTheLeadingSlash);
    RUN_TEST(test_percentEncoding_isDecoded);
    RUN_TEST(test_nullLocation_givesNoResourceFolder_silently);
    RUN_TEST(test_emptyLocation_givesNoResourceFolder_silently);
    RUN_TEST(test_otherSchemes_areRefusedWithAWarning);
    RUN_TEST(test_otherHost_isRefusedWithAWarning);
    RUN_TEST(test_relativePath_isRefusedWithAWarning);
    RUN_TEST(test_malformedPercentEncoding_isRefusedWithAWarning);
    RUN_TEST(test_reset_keepsTheResourceFolder);
    RUN_TEST(test_everyInstance_hasItsOwnResourceFolder);
    RUN_TEST(test_fmuState_doesNotCarryTheResourceFolder);
    return UNITY_END();
}
