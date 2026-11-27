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
#include "integrator.h"

#define IDENTIFIER "validation_model"

typedef struct { double gain; double scaled; int choice; char name[MODEL_STRING_SIZE]; int mode; } Params;
typedef struct { double u; double w; } Inputs;
typedef struct { double y; } Outputs;
typedef struct { double ratio; int level; } Locals;
typedef struct { double x1; double x2; } States;

static void stateEquation(modelInstance* m) { (void)m; }
static void outputEquation(modelInstance* m) { (void)m; }
static void calculateParameters(modelInstance* m) { (void)m; }
static simStatus initialize(modelInstance* m) { (void)m; return SIM_OK; }

/* Enumeration type of the parameter choice (items with a gap) */
static const modelEnumerationItem CHOICE_ITEMS[] = {
    { .name = "one",   .value = 1 },
    { .name = "three", .value = 3 },
};
static const modelEnumeration CHOICE = {
    .name = "Choice", .items = CHOICE_ITEMS, .numberOfItems = ARRAY_LEN(CHOICE_ITEMS),
};

/* Indices into the variable table; X2 is last, so it can be left out */
enum { GAIN, MODE, CHOICE_VAR, NAME, SCALED, U, W, Y, RATIO, LEVEL, X1, X2, NUMBER_OF_VARIABLES };

static const modelVariable VARIABLES[NUMBER_OF_VARIABLES] = {
    [GAIN]   = { PARAM(gain), .start = 1.0, .min = SOME(0.0), .max = SOME(10.0) },
    [MODE]   = { PARAM(mode), .start = 1 },
    [CHOICE_VAR] = { PARAM(choice), .declaredType = &CHOICE, .start = 3 },
    [NAME]   = { PARAM(name), .startString = "table.csv" },
    [SCALED] = { CALCULATED_PARAM(scaled) },
    [U]      = { INPUT(u) },
    [W]      = { INPUT(w) },
    [Y]      = { OUTPUT(y), .dependsOn = "u w" },
    [RATIO]  = { LOCAL(ratio) },
    [LEVEL]  = { LOCAL(level) },
    [X1]     = { STATE(x1), .initial = VAR_APPROX },
    [X2]     = { STATE(x2) },
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
    .sizeOfLocals        = sizeof(Locals),
    .numberOfStates      = NUMBER_OF_DOUBLES(States),
    .getStateDerivatives = stateEquation,
    .getOutputs          = outputEquation,
    .integrator          = &FORWARD_EULER,
    .maxStepSize         = 1e-3,
    .calculateParameters = calculateParameters,
    .initialize          = initialize,
};

/* Mutable copies of the definition and its tables */
static modelDefinition def;
static modelVariable variables[NUMBER_OF_VARIABLES];
static modelUnit units[ARRAY_LEN(UNITS)];
static modelEnumerationItem choiceItems[ARRAY_LEN(CHOICE_ITEMS)];
static modelEnumeration choice;

void setUp(void)
{
    memcpy(variables, VARIABLES, sizeof(VARIABLES));
    memcpy(units, UNITS, sizeof(UNITS));
    memcpy(choiceItems, CHOICE_ITEMS, sizeof(CHOICE_ITEMS));
    choice = CHOICE;
    choice.items = choiceItems;
    def = MODEL_DEFINITION;
    def.variables = variables;
    def.units = units;
    variables[CHOICE_VAR].declaredType = &choice;
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
    def.integrator = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());

    /* an integrator descriptor without a step function */
    static const integratorDef NO_STEP = { .name = "no step", .scratchVectors = 0, .order = 1 };
    setUp();
    def.integrator = &NO_STEP;
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
    /* mode (the last field) no longer fits, gain and scaled still do */
    def.sizeOfParams = offsetof(Params, mode);
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsLocalOutsideItsStorageBlock(void)
{
    /* level (an int at offset 8) no longer fits, ratio still does */
    def.sizeOfLocals = sizeof(double);
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsLocalNameUsedTwice(void)
{
    variables[LEVEL].name = "y";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsCalculatedParameterOutsideTheParameterBlock(void)
{
    variables[SCALED].offset = sizeof(Params);
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsCalculatedParameterWithoutCalculateParameters(void)
{
    def.calculateParameters = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_acceptsBoundsOnVariablesWithoutStartValue(void)
{
    /* outputs, locals and calculated parameters publish no start value, so
     * the start value of the table (0) is not checked against the bounds */
    variables[SCALED].min = (optReal)SOME(1.0);
    variables[Y].min = (optReal)SOME(1.0);
    variables[RATIO].max = (optReal)SOME(-1.0);
    TEST_ASSERT_EQUAL_INT(0, problems());
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

void test_rejectsApproxOnAVariableThatIsNoState(void)
{
    variables[GAIN].initial = VAR_APPROX;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsApproxStateWithoutInitialize(void)
{
    def.initialize = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_acceptsExactStatesWithoutInitialize(void)
{
    variables[X1].initial = VAR_EXACT;
    def.initialize = NULL;
    TEST_ASSERT_EQUAL_INT(0, problems());
}

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
 * Enumerations
 ***************************************************/

void test_rejectsDeclaredTypeOnAVariableThatIsNoInteger(void)
{
    variables[GAIN].declaredType = &choice;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsEnumerationWithoutName(void)
{
    choice.name = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsEnumerationWithoutItems(void)
{
    choice.numberOfItems = 0;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsItemWithoutName(void)
{
    choiceItems[1].name = NULL;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsItemNameUsedTwice(void)
{
    choiceItems[1].name = "one";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsItemValueUsedTwice(void)
{
    choiceItems[0].value = 3;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsTwoTypesWithTheSameName(void)
{
    static const modelEnumerationItem items[] = { { .name = "first", .value = 1 } };
    static const modelEnumeration sameName = { .name = "Choice", .items = items, .numberOfItems = 1 };

    variables[MODE].declaredType = &sameName;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_acceptsOneTypeUsedByTwoVariables(void)
{
    variables[MODE].declaredType = &choice;  /* mode starts at 1, an item */
    TEST_ASSERT_EQUAL_INT(0, problems());
}

void test_rejectsStartValueThatIsNoItem(void)
{
    variables[CHOICE_VAR].start = 2;  /* between the items */
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsBoundsOnAnEnumerationVariable(void)
{
    variables[CHOICE_VAR].max = (optReal)SOME(3);
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_acceptsComputedEnumerationWithoutStartValue(void)
{
    /* a local publishes no start value, so its start (0, no item) is not checked */
    variables[LEVEL].declaredType = &choice;
    TEST_ASSERT_EQUAL_INT(0, problems());
}

/***************************************************
 * Strings
 ***************************************************/

/* A start text of the given length: "aaa...a" */
static char startText[MODEL_STRING_SIZE + 1];
static const char* startTextOfLength(size_t length)
{
    memset(startText, 'a', length);
    startText[length] = '\0';
    return startText;
}

void test_acceptsStringWithoutStartText(void)
{
    variables[NAME].startString = NULL;
    TEST_ASSERT_EQUAL_INT(0, problems());
}

void test_rejectsStringThatIsNoParameter(void)
{
    /* a calculated parameter lives in the parameter block too, so only the role is wrong */
    variables[NAME].role = VAR_CALCULATED_PARAMETER;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsStringOfAnotherSize(void)
{
    variables[NAME].size = MODEL_STRING_SIZE / 2;  /* char name[128] */
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    variables[NAME].size = sizeof(char*);  /* char* name: a pointer, no storage */
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsUnitBoundsNominalAndNumericStartOnAString(void)
{
    variables[NAME].unit = "1";
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    variables[NAME].min = (optReal)SOME(0.0);
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    variables[NAME].max = (optReal)SOME(1.0);
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    variables[NAME].nominal = 1.0;
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    variables[NAME].start = 1.0;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsDeclaredTypeOnAString(void)
{
    variables[NAME].declaredType = &choice;
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_acceptsTheLongestStartText(void)
{
    variables[NAME].startString = startTextOfLength(MODEL_STRING_SIZE - 1);
    TEST_ASSERT_EQUAL_INT(0, problems());
}

void test_rejectsStartTextThatDoesNotFit(void)
{
    variables[NAME].startString = startTextOfLength(MODEL_STRING_SIZE);
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsControlCharacterInTheStartText(void)
{
    variables[NAME].startString = "table\t.csv";
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    variables[NAME].startString = "table\n.csv";
    TEST_ASSERT_EQUAL_INT(1, problems());
    setUp();
    variables[NAME].startString = "\x01\x02";  /* one problem, also for several */
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_acceptsNonAsciiStartText(void)
{
    /* UTF-8 bytes are >= 0x80 and must not be mistaken for control characters */
    variables[NAME].startString = "Tabelle_\xc3\xa4\xc3\xb6\xc3\xbc.csv";
    TEST_ASSERT_EQUAL_INT(0, problems());
}

void test_rejectsStartTextOnAVariableThatIsNoString(void)
{
    variables[GAIN].startString = "1.0";
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

void test_rejectsDependsOnOfALocal(void)
{
    /* locals are not part of the ModelStructure, so they have no dependencies */
    variables[RATIO].dependsOn = "u";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsDependencyOnALocal(void)
{
    variables[Y].dependsOn = "u ratio";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsDependsOnOfACalculatedParameter(void)
{
    variables[SCALED].dependsOn = "u";
    TEST_ASSERT_EQUAL_INT(1, problems());
}

void test_rejectsDependencyOnACalculatedParameter(void)
{
    variables[Y].dependsOn = "u scaled";
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
    RUN_TEST(test_rejectsLocalOutsideItsStorageBlock);
    RUN_TEST(test_rejectsLocalNameUsedTwice);
    RUN_TEST(test_rejectsCalculatedParameterOutsideTheParameterBlock);
    RUN_TEST(test_rejectsCalculatedParameterWithoutCalculateParameters);
    RUN_TEST(test_acceptsBoundsOnVariablesWithoutStartValue);
    RUN_TEST(test_rejectsStartValueOutsideTheBounds);
    RUN_TEST(test_acceptsStartValueOnTheBounds);

    RUN_TEST(test_rejectsApproxOnAVariableThatIsNoState);
    RUN_TEST(test_rejectsApproxStateWithoutInitialize);
    RUN_TEST(test_acceptsExactStatesWithoutInitialize);
    RUN_TEST(test_rejectsStateNotAlignedToTheStateVector);
    RUN_TEST(test_rejectsStateDeclaredTwice);
    RUN_TEST(test_rejectsUndeclaredState);
    RUN_TEST(test_rejectsNumberOfStatesThatDiffersFromTheTable);

    RUN_TEST(test_rejectsDeclaredTypeOnAVariableThatIsNoInteger);
    RUN_TEST(test_rejectsEnumerationWithoutName);
    RUN_TEST(test_rejectsEnumerationWithoutItems);
    RUN_TEST(test_rejectsItemWithoutName);
    RUN_TEST(test_rejectsItemNameUsedTwice);
    RUN_TEST(test_rejectsItemValueUsedTwice);
    RUN_TEST(test_rejectsTwoTypesWithTheSameName);
    RUN_TEST(test_acceptsOneTypeUsedByTwoVariables);
    RUN_TEST(test_rejectsStartValueThatIsNoItem);
    RUN_TEST(test_rejectsBoundsOnAnEnumerationVariable);
    RUN_TEST(test_acceptsComputedEnumerationWithoutStartValue);

    RUN_TEST(test_acceptsStringWithoutStartText);
    RUN_TEST(test_rejectsStringThatIsNoParameter);
    RUN_TEST(test_rejectsStringOfAnotherSize);
    RUN_TEST(test_rejectsUnitBoundsNominalAndNumericStartOnAString);
    RUN_TEST(test_rejectsDeclaredTypeOnAString);
    RUN_TEST(test_acceptsTheLongestStartText);
    RUN_TEST(test_rejectsStartTextThatDoesNotFit);
    RUN_TEST(test_rejectsControlCharacterInTheStartText);
    RUN_TEST(test_acceptsNonAsciiStartText);
    RUN_TEST(test_rejectsStartTextOnAVariableThatIsNoString);

    RUN_TEST(test_rejectsDependsOnOfAVariableThatIsNoOutput);
    RUN_TEST(test_rejectsDependsOnOfALocal);
    RUN_TEST(test_rejectsDependencyOnALocal);
    RUN_TEST(test_rejectsDependsOnOfACalculatedParameter);
    RUN_TEST(test_rejectsDependencyOnACalculatedParameter);
    RUN_TEST(test_rejectsDependencyOnUnknownName);
    RUN_TEST(test_rejectsDependencyOnAVariableThatIsNoInput);
    RUN_TEST(test_rejectsDependencyListedTwice);
    RUN_TEST(test_acceptsEmptyDependencyList);
    RUN_TEST(test_acceptsExtraSpacesInTheDependencyList);
    RUN_TEST(test_doesNotMatchAPrefixOfAnInput);

    return UNITY_END();
}
