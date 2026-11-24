/**
 * Test of the resource path in the model layer: modelSetResourcePath()
 * copies the path and ends it with a '/', resetModel() and the model data
 * leave it alone, and modelOpenFile() opens a UTF-8 path, also one with
 * non-ASCII characters. No FMI involved.
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "model.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

typedef struct { double gain; } Params;

static const modelVariable VARIABLES[] = {
    { PARAM(gain), .start = 2.0 },
};

static const modelDefinition DEFINITION = {
    .name              = "resource_model",
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .sizeOfParams      = sizeof(Params),
};

/* File names for modelOpenFile, in the working directory of the test.
 * "\xc3\xa4" is the UTF-8 encoding of a-umlaut */
#define ASCII_FILE     "csfmu_test_resource.txt"
#define NON_ASCII_FILE "csfmu_test_resource_\xc3\xa4.txt"

static modelInstance* model;

void setUp(void)
{
    model = newModel(&DEFINITION);
    TEST_ASSERT_NOT_NULL(model);
}

void tearDown(void)
{
    freeModel(model);
    model = NULL;
}

/* Remove a file whose name is UTF-8 (remove() takes the local code page on Windows) */
static void removeFile(const char* path)
{
#ifdef _WIN32
    wchar_t wide[64];
    TEST_ASSERT_TRUE(MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 64) > 0);
    _wremove(wide);
#else
    remove(path);
#endif
}

/* Write a text with modelOpenFile, read it back with modelOpenFile */
static void assertRoundTrip(const char* path)
{
    FILE* file = modelOpenFile(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_TRUE(fputs("u,y\n", file) >= 0);
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    char line[16] = "";
    file = modelOpenFile(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), file));
    fclose(file);
    removeFile(path);

    TEST_ASSERT_EQUAL_STRING("u,y\n", line);
}

/***************************************************
 * modelSetResourcePath
 ***************************************************/

void test_newModel_hasNoResourcePath(void)
{
    TEST_ASSERT_NULL(model->resourcePath);
}

void test_resourcePath_endsWithASeparator(void)
{
    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/tmp/fmu/resources"));
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", model->resourcePath);
}

void test_resourcePath_keepsAnExistingSeparator(void)
{
    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/tmp/fmu/resources/"));
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", model->resourcePath);
}

void test_resourcePath_isACopy(void)
{
    char path[] = "/tmp/fmu/resources";
    TEST_ASSERT_TRUE(modelSetResourcePath(model, path));
    path[1] = 'X';  /* the caller may reuse its memory */

    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", model->resourcePath);
}

void test_resourcePath_canBeReplaced_alsoByItself(void)
{
    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/first"));
    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/second"));
    TEST_ASSERT_EQUAL_STRING("/second/", model->resourcePath);

    TEST_ASSERT_TRUE(modelSetResourcePath(model, model->resourcePath));
    TEST_ASSERT_EQUAL_STRING("/second/", model->resourcePath);
}

void test_nullOrEmptyPath_clearsTheResourcePath(void)
{
    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/tmp"));
    TEST_ASSERT_TRUE(modelSetResourcePath(model, NULL));
    TEST_ASSERT_NULL(model->resourcePath);

    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/tmp"));
    TEST_ASSERT_TRUE(modelSetResourcePath(model, ""));
    TEST_ASSERT_NULL(model->resourcePath);
}

void test_resourcePath_refusesANullModel(void)
{
    TEST_ASSERT_FALSE(modelSetResourcePath(NULL, "/tmp"));
}

void test_resetModel_keepsTheResourcePath(void)
{
    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/tmp/fmu/resources"));
    resetModel(model);
    TEST_ASSERT_EQUAL_STRING("/tmp/fmu/resources/", model->resourcePath);
}

void test_resourcePath_isNotPartOfTheModelData(void)
{
    unsigned char buffer[2 * sizeof(double) + sizeof(Params)];
    TEST_ASSERT_EQUAL_size_t(sizeof(buffer), modelDataSize(&DEFINITION));

    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/saved"));
    saveModelData(model, buffer);
    TEST_ASSERT_TRUE(modelSetResourcePath(model, "/current"));
    restoreModelData(model, buffer);

    TEST_ASSERT_EQUAL_STRING("/current/", model->resourcePath);
}

/***************************************************
 * modelOpenFile
 ***************************************************/

void test_modelOpenFile_refusesNullArguments(void)
{
    TEST_ASSERT_NULL(modelOpenFile(NULL, "rb"));
    TEST_ASSERT_NULL(modelOpenFile(ASCII_FILE, NULL));
}

void test_modelOpenFile_failsForAMissingFile(void)
{
    TEST_ASSERT_NULL(modelOpenFile("csfmu_no_such_folder/no_such_file.csv", "rb"));
}

void test_modelOpenFile_opensAnAsciiPath(void)
{
    assertRoundTrip(ASCII_FILE);
}

void test_modelOpenFile_opensANonAsciiPath(void)
{
    assertRoundTrip(NON_ASCII_FILE);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_newModel_hasNoResourcePath);
    RUN_TEST(test_resourcePath_endsWithASeparator);
    RUN_TEST(test_resourcePath_keepsAnExistingSeparator);
    RUN_TEST(test_resourcePath_isACopy);
    RUN_TEST(test_resourcePath_canBeReplaced_alsoByItself);
    RUN_TEST(test_nullOrEmptyPath_clearsTheResourcePath);
    RUN_TEST(test_resourcePath_refusesANullModel);
    RUN_TEST(test_resetModel_keepsTheResourcePath);
    RUN_TEST(test_resourcePath_isNotPartOfTheModelData);
    RUN_TEST(test_modelOpenFile_refusesNullArguments);
    RUN_TEST(test_modelOpenFile_failsForAMissingFile);
    RUN_TEST(test_modelOpenFile_opensAnAsciiPath);
    RUN_TEST(test_modelOpenFile_opensANonAsciiPath);
    return UNITY_END();
}
