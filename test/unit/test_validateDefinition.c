/**
 * Test of the checks of the model definition in the modelDescription.xml
 * generator (validateDefinition in src/fmi2ModelDescription.c).
 *
 * The generator is included as source, so that its static functions can be
 * called; its main() is renamed. Every test starts from a valid definition,
 * injects exactly one mistake and expects exactly one problem, so a test
 * fails if its check does not fire or if another one fires instead.
 *
 * That the generator exits with an error, and thereby stops the build, is
 * tested by test_modelDescription_rejectsWrongIdentifier (CMakeLists.txt).
 */

#define main generator_main
#include "../../src/fmi2ModelDescription.c"
#undef main

#include <math.h>
#include "unity.h"

#define IDENTIFIER "validation_model"

typedef struct { double gain; int mode; } Params;
typedef struct { double u; double w; } Inputs;
typedef struct { double y; } Outputs;
typedef struct { double x1; double x2; } States;

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }
static void integrator(modelInstance* m, double stepSize) { (void)m; (void)stepSize; }

/* Indices into the variable table; X2 is last, so it can be left out */
enum { GAIN, MODE, U, W, Y, X1, X2, NUMBER_OF_VARIABLES };

static const modelVariable VARIABLES[NUMBER_OF_VARIABLES] = {
    [GAIN] = { PARAM(gain), .start = 1.0, .min = SOME(0.0), .max = SOME(10.0) },
    [MODE] = { PARAM(mode), .start = 1 },
    [U]    = { INPUT(u) },
    [W]    = { INPUT(w) },
    [Y]    = { OUTPUT(y), .dependsOn = "u w" },
    [X1]   = { STATE(x1) },
    [X2]   = { STATE(x2) },
};

static const modelUnit UNITS[] = {
    { .name = "1" },
};

/* The valid definition every test starts from (also used by generator_main) */
const modelDefinition MODEL_DEFINITION = {
    .name                = IDENTIFIER,
    .guid                = "{00000000-0000-0000-0000-000000000002}",
    .variables           = VARIABLES,
    .numberOfVariables   = ARRAY_LEN(VARIABLES),
    .units               = UNITS,
    .numberOfUnits       = ARRAY_LEN(UNITS),
    .sizeOfParams        = sizeof(Params),
    .sizeOfInputs        = sizeof(Inputs),
    .sizeOfOutputs       = sizeof(Outputs),
    .numberOfStates      = NUMBER_OF_DOUBLES(States),
    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrate           = integrator,
    .maxStepSize         = 1e-3,
};

/* Mutable copies of the definition and its tables */
static modelDefinition def;
static modelVariable variables[NUMBER_OF_VARIABLES];
static modelUnit units[ARRAY_LEN(UNITS)];

void setUp(void)
{
    memcpy(variables, VARIABLES, sizeof(VARIABLES));
    memcpy(units, UNITS, sizeof(UNITS));
    def = MODEL_DEFINITION;
    def.variables = variables;
    def.units = units;
}

void tearDown(void) {}

static int problems(void)
{
    return validateDefinition(&def, IDENTIFIER);
}

/***************************************************
 * The definition as a whole
 ***************************************************/

void test_validDefinitionHasNoProblem(void)
{
    TEST_ASSERT_EQUAL_INT(0, problems());
}

void test_rejectsMissingName(void)
{
    def.name = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsMissingGuid(void)
{
    def.guid = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsNameThatIsNoIdentifier(void)
{
    /* the identifier is the same, so only the identifier check can fire */
    def.name = "2fast";
    TEST_ASSERT_EQUAL_INT(1, validateDefinition(&def, "2fast"));
    def.name = "my-model";
    TEST_ASSERT_EQUAL_INT(1, validateDefinition(&def, "my-model"));
}

void test_rejectsNameThatDiffersFromTheIdentifier(void)
{
    TEST_ASSERT_EQUAL_INT(1, validateDefinition(&def, "another_name"));
}

void test_rejectsMissingEquations(void)
{
    def.getStateDerivatives = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    def.getOutputs = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    def.integrate = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsMaxStepSizeThatIsNotPositive(void)
{
    def.maxStepSize = 0.0;
    TEST_ASSERT_EQUAL_INT(1, problems());
    def.maxStepSize = -1e-3;
    TEST_ASSERT_EQUAL_INT(1, problems());
    def.maxStepSize = NAN;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsUnitWithoutName(void)
{
    units[0].name = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

/***************************************************
 * Variables
 ***************************************************/

void test_rejectsVariableWithoutName(void)
{
    variables[MODE].name = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsNameUsedTwice(void)
{
    variables[MODE].name = "gain";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsVariableOutsideItsStorageBlock(void)
{
    /* mode (an int at offset 8) no longer fits, gain still does */
    def.sizeOfParams = sizeof(double);
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsStartValueOutsideTheBounds(void)
{
    variables[GAIN].start = -1.0;
    TEST_ASSERT_EQUAL_INT(1, problems());
    variables[GAIN].start = 11.0;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_acceptsStartValueOnTheBounds(void)
{
    variables[GAIN].start = 0.0;
    TEST_ASSERT_EQUAL_INT(0, problems());
    variables[GAIN].start = 10.0;
    TEST_ASSERT_EQUAL_INT(0, problems());
}

/***************************************************
 * States
 ***************************************************/

void test_rejectsStateNotAlignedToTheStateVector(void)
{
    variables[X2].offset = sizeof(double) / 2;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsStateDeclaredTwice(void)
{
    variables[X2].offset = variables[X1].offset;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsUndeclaredState(void)
{
    def.numberOfVariables = NUMBER_OF_VARIABLES - 1;  /* without x2 */
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsNumberOfStatesThatDiffersFromTheTable(void)
{
    def.numberOfStates = 3;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

/***************************************************
 * dependsOn
 ***************************************************/

void test_rejectsDependsOnOfAVariableThatIsNoOutput(void)
{
    variables[U].dependsOn = "w";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsDependencyOnUnknownName(void)
{
    variables[Y].dependsOn = "u no_such_input";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsDependencyOnAVariableThatIsNoInput(void)
{
    variables[Y].dependsOn = "gain";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsDependencyListedTwice(void)
{
    variables[Y].dependsOn = "u w u";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_acceptsEmptyDependencyList(void)
{
    variables[Y].dependsOn = "";
    TEST_ASSERT_EQUAL_INT(0, problems());
}

void test_acceptsExtraSpacesInTheDependencyList(void)
{
    variables[Y].dependsOn = "  w   u ";
    TEST_ASSERT_EQUAL_INT(0, problems());
}

void test_doesNotMatchAPrefixOfAnInput(void)
{
    /* "u" is an input, "uw" is not */
    variables[Y].dependsOn = "uw";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_validDefinitionHasNoProblem);
    RUN_TEST(test_rejectsMissingName);
    RUN_TEST(test_rejectsMissingGuid);
    RUN_TEST(test_rejectsNameThatIsNoIdentifier);
    RUN_TEST(test_rejectsNameThatDiffersFromTheIdentifier);
    RUN_TEST(test_rejectsMissingEquations);
    RUN_TEST(test_rejectsMaxStepSizeThatIsNotPositive);
    RUN_TEST(test_rejectsUnitWithoutName);

    RUN_TEST(test_rejectsVariableWithoutName);
    RUN_TEST(test_rejectsNameUsedTwice);
    RUN_TEST(test_rejectsVariableOutsideItsStorageBlock);
    RUN_TEST(test_rejectsStartValueOutsideTheBounds);
    RUN_TEST(test_acceptsStartValueOnTheBounds);

    RUN_TEST(test_rejectsStateNotAlignedToTheStateVector);
    RUN_TEST(test_rejectsStateDeclaredTwice);
    RUN_TEST(test_rejectsUndeclaredState);
    RUN_TEST(test_rejectsNumberOfStatesThatDiffersFromTheTable);

    RUN_TEST(test_rejectsDependsOnOfAVariableThatIsNoOutput);
    RUN_TEST(test_rejectsDependencyOnUnknownName);
    RUN_TEST(test_rejectsDependencyOnAVariableThatIsNoInput);
    RUN_TEST(test_rejectsDependencyListedTwice);
    RUN_TEST(test_acceptsEmptyDependencyList);
    RUN_TEST(test_acceptsExtraSpacesInTheDependencyList);
    RUN_TEST(test_doesNotMatchAPrefixOfAnInput);

    return UNITY_END();
}
