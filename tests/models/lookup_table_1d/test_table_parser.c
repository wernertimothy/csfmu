/**
 * C-level test of reading the table file of the lookup_table_1d model. It
 * drives the model layer directly (no FMI, no FMPy), so that the sanitizer
 * job (ASan/UBSan) covers the parser in equations.c: its fixed-size line and
 * number buffers, strtod() and the handling of the locale. The cases at the
 * limits of these buffers are the ones a sanitizer can catch.
 *
 * The Python tests in test_table_file.py and test_interpolation.py cover the
 * same behavior through FMPy.
 *
 * PACKAGED_RESOURCES (the resources folder of the model) and TABLE_DIR (the
 * build folder of this test, where it writes its own tables) are given by
 * CMakeLists.txt; both end with '/'.
 */

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "model.h"

/* Room for the text of the longest table this test writes (257 points) */
#define TEXT_SIZE 4096

/* Largest number of table files one test writes */
#define MAX_WRITTEN 8

static modelInstance* model;

static char writtenNames[MAX_WRITTEN][64];
static size_t numberOfWritten;

/* The locale before a test switched it, "" if it did not */
static char previousLocale[128];

/* Index of a variable in the variable table, looked up by name (never hardcoded) */
static size_t indexOf(const char* name)
{
    for (size_t k = 0; k < MODEL_DEFINITION.numberOfVariables; k++) {
        if (strcmp(MODEL_DEFINITION.variables[k].name, name) == 0) {
            return k;
        }
    }
    TEST_FAIL_MESSAGE("unknown variable");
    return 0;
}

static void pathOf(char* path, size_t size, const char* name)
{
    int length = snprintf(path, size, "%s%s", TABLE_DIR, name);
    TEST_ASSERT_TRUE_MESSAGE(length > 0 && (size_t)length < size, "path too long");
}

/* Writes a table file into TABLE_DIR (removed by tearDown) and returns its name */
static const char* writeTable(const char* name, const char* text)
{
    char path[1024];
    pathOf(path, sizeof(path), name);

    FILE* file = modelOpenFile(path, "wb");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, path);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    /* remember every name once, also if the file is written again */
    for (size_t k = 0; k < numberOfWritten; k++) {
        if (strcmp(writtenNames[k], name) == 0) {
            return name;
        }
    }
    TEST_ASSERT_TRUE(numberOfWritten < MAX_WRITTEN);
    snprintf(writtenNames[numberOfWritten++], sizeof(writtenNames[0]), "%s", name);
    return name;
}

static void removeTable(const char* name)
{
    char path[1024];
    pathOf(path, sizeof(path), name);
    remove(path);
}

/* Initializes the model with the table file name in the folder directory */
static simStatus start(const char* directory, const char* tableFile)
{
    TEST_ASSERT_TRUE(modelSetResourcePath(model, directory));
    TEST_ASSERT_TRUE(modelSetString(model, indexOf("table_file"), tableFile));
    return initializeModel(model);
}

/* y for the input u */
static double y(double u)
{
    *(double*)modelVariableAddress(model, indexOf("u")) = u;
    model->getOutputs(model);
    return *(const double*)modelVariableAddress(model, indexOf("y"));
}

/* Initializing with a table of the given text must fail with a message that contains both parts */
static void assertTableFails(const char* text, const char* part1, const char* part2)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_ERROR, start(TABLE_DIR, writeTable("failing.csv", text)), text);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(model->message, part1), model->message);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(model->message, part2), model->message);
}

void setUp(void)
{
    model = newModel(&MODEL_DEFINITION);
    TEST_ASSERT_NOT_NULL(model);
    numberOfWritten = 0;
    previousLocale[0] = '\0';
}

void tearDown(void)
{
    if (previousLocale[0] != '\0') {
        setlocale(LC_NUMERIC, previousLocale);
    }
    for (size_t k = 0; k < numberOfWritten; k++) {
        removeTable(writtenNames[k]);
    }
    freeModel(model);
}

void test_packagedTable_isInterpolatedAndClamped(void)
{
    /* (u, y): the points, the middle of every segment, and clamping on both sides */
    static const double EXPECTED[][2] = {
        { -2.0, -2.0 }, { -1.0, -2.0 }, { -0.5, -1.0 }, { 0.0, 0.0 }, { 0.5, 0.5 }, { 1.0, 1.0 },
        { 1.5, 1.25 }, { 2.0, 1.5 }, { 2.5, 1.625 }, { 3.0, 1.75 }, { 4.0, 1.75 },
    };

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(PACKAGED_RESOURCES, "table.csv"), model->message);
    for (size_t k = 0; k < ARRAY_LEN(EXPECTED); k++) {
        TEST_ASSERT_EQUAL_DOUBLE(EXPECTED[k][1], y(EXPECTED[k][0]));
    }
}

void test_windowsLineBreaksBlankLinesAndBlanks_areAccepted(void)
{
    const char* text = "u,y\r\n\r\n 0 , 0 \r\n\t1.0,\t1e1\r\n\r\n2.5E0 , -2.5e-1\r\n\r\n";
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(TABLE_DIR, writeTable("relaxed.csv", text)), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, y(0.5));
    TEST_ASSERT_EQUAL_DOUBLE(-0.25, y(2.5));
}

void test_lastLineWithoutLineBreak_isRead(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(TABLE_DIR, writeTable("no_break.csv", "u,y\n0,0\n1,4")),
                                  model->message);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, y(0.25));
}

void test_malformedLines_fail(void)
{
    static const char* LINES[] = {
        "1;1", "1,1,1", "1,abc", "abc,1", "1,", ",1", "1 2", "1,1x", "0x1,1", "inf,1", "nan,1", "1,2,",
    };

    for (size_t k = 0; k < ARRAY_LEN(LINES); k++) {
        char text[64];
        snprintf(text, sizeof(text), "u,y\n0,0\n%s\n", LINES[k]);
        assertTableFails(text, "Line 3 of the table file", "is not a point");
    }
}

/* A number has at most 63 characters (NUMBER_SIZE - 1 in equations.c) */
void test_numberAtTheLimitOfItsBuffer(void)
{
    char longest[64];   /* 62 zeros and a 1: 63 characters */
    memset(longest, '0', 62);
    longest[62] = '1';
    longest[63] = '\0';

    char text[256];
    snprintf(text, sizeof(text), "u,y\n0,0\n%s,2\n", longest);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(TABLE_DIR, writeTable("longest_number.csv", text)), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, y(0.5));

    snprintf(text, sizeof(text), "u,y\n0,0\n0%s,2\n", longest);   /* 64 characters */
    assertTableFails(text, "Line 3 of the table file", "is not a point");
}

/* A line has at most 254 characters besides its line break (LINE_SIZE - 2 in equations.c) */
void test_lineAtTheLimitOfItsBuffer(void)
{
    char line[300];
    char text[400];

    /* "0," + 251 blanks + "0" = 254 characters */
    memcpy(line, "0,", 2);
    memset(line + 2, ' ', 251);
    memcpy(line + 253, "0", 2);
    TEST_ASSERT_EQUAL_size_t(254, strlen(line));

    snprintf(text, sizeof(text), "u,y\n%s\n1,1\n", line);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(TABLE_DIR, writeTable("longest_line.csv", text)), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(0.5, y(0.5));

    /* one blank more: 255 characters */
    snprintf(text, sizeof(text), "u,y\n %s\n1,1\n", line);
    assertTableFails(text, "Line 2 of the table file", "is longer than 254 characters");
}

void test_lineThatIsFarTooLong_fails(void)
{
    char text[400];
    char zeros[301];
    memset(zeros, '0', 300);
    zeros[300] = '\0';

    snprintf(text, sizeof(text), "u,y\n0,%s\n1,1\n", zeros);
    assertTableFails(text, "Line 2 of the table file", "is longer than");
}

void test_valueThatIsNotFinite_fails(void)
{
    assertTableFails("u,y\n0,0\n1,1e999\n", "Line 3 of the table file", "not finite");
}

void test_uMustIncreaseStrictly(void)
{
    assertTableFails("u,y\n0,0\n1,1\n1,2\n", "Line 4 of the table file", "strictly increasing");
    assertTableFails("u,y\n0,0\n2,1\n1,2\n", "Line 4 of the table file", "strictly increasing");
}

void test_firstLineMustBeAHeader(void)
{
    assertTableFails("0,0\n1,1\n2,2\n", "first line", "must be a header");
}

void test_tableNeedsTwoPoints(void)
{
    assertTableFails("u,y\n", "has 0 point(s)", "at least 2 are needed");
    assertTableFails("u,y\n0,0\n", "has 1 point(s)", "at least 2 are needed");
    assertTableFails("", "has 0 point(s)", "at least 2 are needed");
}

/* Writes a header and the points (k, 2k) for k = 0 .. points - 1 into text */
static void rampTable(char* text, size_t size, int points)
{
    size_t length = (size_t)snprintf(text, size, "u,y\n");
    for (int k = 0; k < points; k++) {
        length += (size_t)snprintf(text + length, size - length, "%d,%d\n", k, 2 * k);
        TEST_ASSERT_TRUE(length < size);
    }
}

/* At most 256 points (LOOKUP_TABLE_1D_MAX_POINTS) fit into the internals */
void test_tableAtTheLimitOfItsPoints(void)
{
    static char text[TEXT_SIZE];

    rampTable(text, sizeof(text), 256);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(TABLE_DIR, writeTable("256_points.csv", text)), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(509.0, y(254.5));
    TEST_ASSERT_EQUAL_DOUBLE(510.0, y(1000.0));

    rampTable(text, sizeof(text), 257);
    assertTableFails(text, "has more than 256 points", "");
}

void test_missingFile_failsWithItsPath(void)
{
    char expected[1024];
    snprintf(expected, sizeof(expected), "Cannot open the table file '%smissing.csv'", TABLE_DIR);

    TEST_ASSERT_EQUAL_INT(SIM_ERROR, start(TABLE_DIR, "missing.csv"));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(model->message, expected), model->message);
}

void test_withoutResourceFolder_fails(void)
{
    TEST_ASSERT_TRUE(modelSetResourcePath(model, NULL));
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, initializeModel(model));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(model->message, "has no resource folder"), model->message);
}

void test_emptyTableFile_fails(void)
{
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, start(TABLE_DIR, ""));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(model->message, "table_file is empty"), model->message);
}

/* The table is cached in the internals: an initialization run reads the file
   only for another table_file, and resetModel() forgets the table */
void test_tableIsCachedUntilAnotherFileIsNamed(void)
{
    const char* cached = writeTable("cached.csv", "u,y\n0,0\n1,10\n");
    const char* other = writeTable("other.csv", "u,y\n0,0\n1,100\n");

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(TABLE_DIR, cached), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(5.0, y(0.5));

    removeTable(cached);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, initializeModel(model), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(2.5, y(0.25));

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(TABLE_DIR, other), model->message);
    TEST_ASSERT_EQUAL_DOUBLE(50.0, y(0.5));

    resetModel(model);
    TEST_ASSERT_EQUAL_INT(SIM_ERROR, start(TABLE_DIR, cached));
}

/* The table file always uses '.', whatever the locale of the C library is */
void test_tableReadsTheSameInACommaLocale(void)
{
    static const char* COMMA_LOCALES[] = { "de_DE.UTF-8", "de_DE.utf8", "de_DE", "German_Germany.1252", "German" };

    snprintf(previousLocale, sizeof(previousLocale), "%s", setlocale(LC_NUMERIC, NULL));
    bool switched = false;
    for (size_t k = 0; k < ARRAY_LEN(COMMA_LOCALES) && !switched; k++) {
        switched = setlocale(LC_NUMERIC, COMMA_LOCALES[k]) && localeconv()->decimal_point[0] == ',';
    }
    if (!switched) {
        TEST_IGNORE_MESSAGE("no locale with ',' as decimal point is installed");
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(SIM_OK, start(TABLE_DIR, writeTable("decimal.csv", "u,y\n0,0\n0.5,0.25\n1,1\n")),
                                  model->message);
    TEST_ASSERT_EQUAL_DOUBLE(0.25, y(0.5));
    TEST_ASSERT_EQUAL_DOUBLE(0.625, y(0.75));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_packagedTable_isInterpolatedAndClamped);
    RUN_TEST(test_windowsLineBreaksBlankLinesAndBlanks_areAccepted);
    RUN_TEST(test_lastLineWithoutLineBreak_isRead);
    RUN_TEST(test_malformedLines_fail);
    RUN_TEST(test_numberAtTheLimitOfItsBuffer);
    RUN_TEST(test_lineAtTheLimitOfItsBuffer);
    RUN_TEST(test_lineThatIsFarTooLong_fails);
    RUN_TEST(test_valueThatIsNotFinite_fails);
    RUN_TEST(test_uMustIncreaseStrictly);
    RUN_TEST(test_firstLineMustBeAHeader);
    RUN_TEST(test_tableNeedsTwoPoints);
    RUN_TEST(test_tableAtTheLimitOfItsPoints);
    RUN_TEST(test_missingFile_failsWithItsPath);
    RUN_TEST(test_withoutResourceFolder_fails);
    RUN_TEST(test_emptyTableFile_fails);
    RUN_TEST(test_tableIsCachedUntilAnotherFileIsNamed);
    RUN_TEST(test_tableReadsTheSameInACommaLocale);
    return UNITY_END();
}
