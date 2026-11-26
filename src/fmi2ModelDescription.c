/**
 * @file fmi2ModelDescription.c
 * @brief Generates the FMI 2.0 modelDescription.xml from the model definition.
 *
 * This program is built and run as part of the FMU build. It is linked
 * against the selected model and translates its MODEL_DEFINITION into
 * FMI 2.0 terms:
 *
 * - The value reference of a variable is its index in the variable table.
 *   fmi2Functions.c uses the same mapping.
 * - The role of a variable determines causality, variability and initial.
 * - Local variables are published (readable), but are not part of the
 *   ModelStructure.
 * - Calculated parameters are initial unknowns of the ModelStructure.
 * - The enumeration types the variables use become TypeDefinitions.
 * - String parameters are published with their start text.
 * - The inputs an output depends on directly (dependsOn) become the
 *   dependencies of the output in the ModelStructure.
 * - The capability flags and log categories describe fmi2Functions.c and
 *   are therefore defined here, not in the model.
 *
 * The model identifier is the name of the FMU binary. It is passed in by
 * the build and must be equal to the name of the model definition.
 *
 * Usage: generate_model_description <output file> <model identifier>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"

/***************************************************
 * Capabilities of the FMI adapter (fmi2Functions.c)
 ***************************************************/

#define CAN_HANDLE_VARIABLE_COMMUNICATION_STEP_SIZE "true"
#define CAN_INTERPOLATE_INPUTS                      "false"
#define MAX_OUTPUT_DERIVATIVE_ORDER                 "0"
#define CAN_GET_AND_SET_FMU_STATE                   "true"
#define CAN_SERIALIZE_FMU_STATE                     "true"
#define PROVIDES_DIRECTIONAL_DERIVATIVE             "false"
/* The adapter allocates with calloc() and ignores the memory callbacks */
#define CAN_NOT_USE_MEMORY_MANAGEMENT_FUNCTIONS     "true"

/***************************************************
 * Helper Functions
 ***************************************************/

/**
 * Size in bytes of a variable of the given type
 */
static size_t sizeOfType(varType type)
{
    switch (type) {
        case VAR_REAL:    return sizeof(double);
        case VAR_INTEGER: return sizeof(int);
        case VAR_BOOLEAN: return sizeof(bool);
        case VAR_STRING:  return MODEL_STRING_SIZE;
    }
    return 0;
}

/**
 * Size in bytes of the storage block a variable of the given role lives in
 */
static size_t sizeOfBlock(const modelDefinition* def, varRole role)
{
    switch (role) {
        case VAR_PARAMETER: return def->sizeOfParams;
        case VAR_INPUT:     return def->sizeOfInputs;
        case VAR_OUTPUT:    return def->sizeOfOutputs;
        case VAR_STATE:     return def->numberOfStates * sizeof(double);
        case VAR_LOCAL:     return def->sizeOfLocals;
        case VAR_CALCULATED_PARAMETER: return def->sizeOfParams;
    }
    return 0;
}

/**
 * Whether variable k is the first variable of the table with its
 * enumeration type (so the type is written and validated only once)
 */
static bool isFirstUseOfType(const modelDefinition* def, size_t k)
{
    const modelEnumeration* type = def->variables[k].declaredType;

    for (size_t j = 0; j < k; j++) {
        if (def->variables[j].declaredType == type) {
            return false;
        }
    }
    return true;
}

/**
 * Whether variables of the given role publish a start value. Outputs, local
 * variables and calculated parameters are computed by the model
 * (initial="calculated"), so they have none.
 */
static bool hasStartValue(varRole role)
{
    return role == VAR_PARAMETER || role == VAR_INPUT || role == VAR_STATE;
}

/**
 * Write a string as XML attribute value (escapes special characters)
 */
static void writeEscaped(FILE* file, const char* text)
{
    for (const char* c = text; *c != '\0'; c++) {
        switch (*c) {
            case '&':  fputs("&amp;", file);  break;
            case '<':  fputs("&lt;", file);   break;
            case '>':  fputs("&gt;", file);   break;
            case '"':  fputs("&quot;", file); break;
            default:   fputc(*c, file);       break;
        }
    }
}

/**
 * Write an optional string attribute (nothing is written if text is NULL)
 */
static void writeStringAttribute(FILE* file, const char* name, const char* text)
{
    if (text) {
        fprintf(file, " %s=\"", name);
        writeEscaped(file, text);
        fputs("\"", file);
    }
}

/**
 * Write a real attribute using the shortest representation that reads
 * back as exactly the same double (0.15 instead of 0.14999999999999999)
 */
static void writeRealAttribute(FILE* file, const char* name, double value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.15g", value);
    if (strtod(buffer, NULL) != value) {
        snprintf(buffer, sizeof(buffer), "%.17g", value);
    }
    fprintf(file, " %s=\"%s\"", name, buffer);
}

/**
 * Write a base unit exponent (nothing is written if the exponent is 0)
 */
static void writeExponent(FILE* file, const char* name, int exponent)
{
    if (exponent != 0) {
        fprintf(file, " %s=\"%d\"", name, exponent);
    }
}

/**
 * Check whether a unit name is defined in the model's unit table
 */
static bool unitIsDefined(const modelDefinition* def, const char* unit)
{
    for (size_t k = 0; k < def->numberOfUnits; k++) {
        if (strcmp(def->units[k].name, unit) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Step through a list of names that are separated by spaces
 *
 * Returns the start of the next name and stores its length, or returns
 * NULL if there is no further name. Continue with the returned position
 * plus the length.
 */
static const char* nextName(const char* position, size_t* length)
{
    while (*position == ' ') {
        position++;
    }
    if (*position == '\0') {
        return NULL;
    }

    *length = strcspn(position, " ");
    return position;
}

/**
 * Check whether a list of names (separated by spaces) contains a name
 */
static bool nameListContains(const char* list, const char* name)
{
    size_t length = 0;

    for (const char* entry = nextName(list, &length); entry; entry = nextName(entry + length, &length)) {
        if (strlen(name) == length && strncmp(entry, name, length) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Check whether a name is a valid C identifier (required for the model
 * identifier, which is also the name of the FMU binary)
 */
static bool isIdentifier(const char* name)
{
    if (name[0] == '\0' || (name[0] >= '0' && name[0] <= '9')) {
        return false;
    }
    for (const char* c = name; *c != '\0'; c++) {
        bool isLetter = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z');
        bool isDigit = (*c >= '0' && *c <= '9');
        if (!isLetter && !isDigit && *c != '_') {
            return false;
        }
    }
    return true;
}

/***************************************************
 * Validation
 ***************************************************/

/**
 * Check the dependsOn list of one variable
 *
 * Prints one message per problem and returns the number of problems.
 */
static int validateDependencies(const modelDefinition* def, const modelVariable* variable)
{
    int problems = 0;

    if (variable->role != VAR_OUTPUT) {
        fprintf(stderr, "error: '%s' is not an output, only outputs can have dependsOn\n", variable->name);
        return 1;
    }

    size_t length = 0;

    for (const char* entry = nextName(variable->dependsOn, &length); entry;
         entry = nextName(entry + length, &length)) {

        /* The name must be an input of this model */
        bool isInput = false;
        for (size_t k = 0; k < def->numberOfVariables; k++) {
            const modelVariable* other = &def->variables[k];
            if (other->role == VAR_INPUT && other->name &&
                strlen(other->name) == length && strncmp(other->name, entry, length) == 0) {
                isInput = true;
            }
        }
        if (!isInput) {
            fprintf(stderr, "error: output '%s' depends on '%.*s', which is not an input of this model "
                            "(dependsOn takes the published names of inputs, separated by spaces)\n",
                            variable->name, (int)length, entry);
            problems++;
        }

        /* The name must not have been listed before */
        size_t earlierLength = 0;
        for (const char* earlier = nextName(variable->dependsOn, &earlierLength); earlier != entry;
             earlier = nextName(earlier + earlierLength, &earlierLength)) {
            if (earlierLength == length && strncmp(earlier, entry, length) == 0) {
                fprintf(stderr, "error: output '%s' lists '%.*s' more than once in dependsOn\n",
                                variable->name, (int)length, entry);
                problems++;
                break;
            }
        }
    }

    return problems;
}

/**
 * Check an enumeration type, the first time a variable uses it
 *
 * Prints one message per problem and returns the number of problems.
 */
static int validateEnumeration(const modelDefinition* def, size_t k)
{
    const modelEnumeration* type = def->variables[k].declaredType;
    int problems = 0;

    if (!type->name) {
        fprintf(stderr, "error: the enumeration type of '%s' has no name\n", def->variables[k].name);
        return 1;
    }

    /* Type names must be unique: a different type with the same name, used earlier */
    for (size_t j = 0; j < k; j++) {
        const modelEnumeration* other = def->variables[j].declaredType;
        if (other && other != type && isFirstUseOfType(def, j) && other->name &&
            strcmp(other->name, type->name) == 0) {
            fprintf(stderr, "error: two different enumeration types are named '%s'\n", type->name);
            problems++;
        }
    }

    if (type->numberOfItems == 0 || !type->items) {
        fprintf(stderr, "error: enumeration '%s' has no items\n", type->name);
        return problems + 1;
    }

    for (size_t i = 0; i < type->numberOfItems; i++) {
        const modelEnumerationItem* item = &type->items[i];

        if (!item->name) {
            fprintf(stderr, "error: item %zu of enumeration '%s' has no name\n", i, type->name);
            problems++;
            continue;
        }
        for (size_t j = 0; j < i; j++) {
            const modelEnumerationItem* earlier = &type->items[j];
            if (earlier->name && strcmp(earlier->name, item->name) == 0) {
                fprintf(stderr, "error: enumeration '%s' has more than one item named '%s'\n",
                        type->name, item->name);
                problems++;
            }
            if (earlier->value == item->value) {
                fprintf(stderr, "error: items '%s' and '%s' of enumeration '%s' have the same value %d\n",
                        earlier->name ? earlier->name : "?", item->name, type->name, item->value);
                problems++;
            }
        }
    }

    return problems;
}

/**
 * Check a string variable
 *
 * Prints one message per problem and returns the number of problems.
 *
 * The start text is checked byte by byte: control characters cannot be
 * published, because an XML parser replaces tab, line feed and carriage
 * return in an attribute value by spaces (attribute-value normalization),
 * and the other control characters are not allowed in XML 1.0 at all.
 */
static int validateString(const modelVariable* variable)
{
    int problems = 0;

    if (variable->role != VAR_PARAMETER) {
        fprintf(stderr, "error: '%s' is a string, but only parameters can be strings\n", variable->name);
        problems++;
    }

    if (variable->size != MODEL_STRING_SIZE) {
        fprintf(stderr, "error: string parameter '%s' must be declared as char %s[MODEL_STRING_SIZE] "
                        "(a char* pointer or an array of another size cannot be published)\n",
                        variable->name, variable->name);
        problems++;
    }

    if (variable->unit || variable->min.set || variable->max.set || variable->nominal != 0.0 ||
        variable->start != 0.0) {
        fprintf(stderr, "error: string parameter '%s' has .unit, .min, .max, .nominal or .start; "
                        "its start value is the text .startString\n", variable->name);
        problems++;
    }

    if (variable->startString) {
        size_t length = strlen(variable->startString);
        if (length >= MODEL_STRING_SIZE) {
            fprintf(stderr, "error: the start text of '%s' has %zu bytes, at most %d fit "
                            "(MODEL_STRING_SIZE - 1)\n", variable->name, length, MODEL_STRING_SIZE - 1);
            problems++;
        }
        for (size_t k = 0; k < length; k++) {
            if ((unsigned char)variable->startString[k] < 0x20) {
                fprintf(stderr, "error: the start text of '%s' contains the control character %d at "
                                "byte %zu, which the modelDescription.xml cannot hold\n",
                                variable->name, (unsigned char)variable->startString[k], k);
                problems++;
                break;
            }
        }
    }

    return problems;
}

/**
 * Check the model definition for mistakes that the compiler cannot catch
 *
 * Prints one message per problem and returns the number of problems.
 *
 * Checks:
 * - Required fields of the definition are set
 * - The name of the definition is the model identifier
 * - Variable names are present and unique
 * - Every variable lies inside its storage block
 * - The states cover the state vector exactly (every double of the struct
 *   States is published once)
 * - Start values lie within [min, max] (for variables that have one)
 * - dependsOn is only used by outputs and names inputs of this model
 * - initial = VAR_APPROX is only used by states, and only if the model has
 *   an initialize function that can replace the start value
 * - A model with calculated parameters has a calculateParameters function
 * - Enumeration types are only used by Integer variables, have a unique
 *   name and items with unique names and values; an enumeration variable
 *   starts at the value of an item and has no bounds of its own
 * - Strings are parameters declared as char[MODEL_STRING_SIZE], without unit,
 *   bounds, nominal or numeric start; their start text fits and has no
 *   control characters. Only strings have a start text
 */
static int validateDefinition(const modelDefinition* def, const char* modelIdentifier)
{
    int problems = 0;

    if (!def->name || !def->guid) {
        fprintf(stderr, "error: model definition needs a name and a guid\n");
        problems++;
    }
    if (def->name && !isIdentifier(def->name)) {
        fprintf(stderr, "error: the model name '%s' is not a valid C identifier "
                        "(letters, digits and underscores, not starting with a digit)\n", def->name);
        problems++;
    }
    if (def->name && strcmp(def->name, modelIdentifier) != 0) {
        fprintf(stderr, "error: the model definition is named '%s' but the FMU binary is named '%s' "
                        "(.name must be equal to the name of the model folder)\n",
                        def->name, modelIdentifier);
        problems++;
    }
    if (!def->getStateDerivatives || !def->getOutputs || !def->integrate) {
        fprintf(stderr, "error: model definition needs getStateDerivatives, getOutputs and integrate\n");
        problems++;
    }
    if (!(def->maxStepSize > 0.0)) {
        fprintf(stderr, "error: maxStepSize must be greater than zero\n");
        problems++;
    }
    for (size_t k = 0; k < def->numberOfUnits; k++) {
        if (!def->units[k].name) {
            fprintf(stderr, "error: unit %zu has no name\n", k);
            problems++;
        }
    }

    size_t numberOfStateVariables = 0;
    bool hasCalculatedParameters = false;

    for (size_t k = 0; k < def->numberOfVariables; k++) {
        const modelVariable* variable = &def->variables[k];

        if (!variable->name) {
            fprintf(stderr, "error: variable %zu has no name\n", k);
            problems++;
            continue;
        }

        /* Names must be unique */
        for (size_t j = 0; j < k; j++) {
            if (def->variables[j].name && strcmp(def->variables[j].name, variable->name) == 0) {
                fprintf(stderr, "error: variable name '%s' is used more than once "
                                "(use PARAM_AS, CALCULATED_PARAM_AS, INPUT_AS, OUTPUT_AS, LOCAL_AS or "
                                "STATE_AS to publish a field under a different name)\n", variable->name);
                problems++;
            }
        }

        /* Variable must lie inside its storage block (a string of the wrong
           size is reported by validateString instead) */
        bool isStringOfWrongSize = variable->type == VAR_STRING && variable->size != MODEL_STRING_SIZE;
        if (!isStringOfWrongSize &&
            variable->offset + sizeOfType(variable->type) > sizeOfBlock(def, variable->role)) {
            fprintf(stderr, "error: variable '%s' lies outside its storage block "
                            "(check sizeOfParams, sizeOfInputs, sizeOfOutputs, sizeOfLocals, numberOfStates)\n",
                            variable->name);
            problems++;
        }

        /* States must be distinct entries of the state vector */
        if (variable->role == VAR_STATE) {
            numberOfStateVariables++;
            if (variable->offset % sizeof(double) != 0) {
                fprintf(stderr, "error: state '%s' is not aligned to the state vector "
                                "(the struct States must contain only doubles)\n", variable->name);
                problems++;
            }
            for (size_t j = 0; j < k; j++) {
                if (def->variables[j].role == VAR_STATE && def->variables[j].offset == variable->offset) {
                    fprintf(stderr, "error: state '%s' is declared more than once\n", variable->name);
                    problems++;
                }
            }
        }

        /* Start value must respect the bounds (a string has neither, see validateString) */
        if (hasStartValue(variable->role) && variable->type != VAR_STRING &&
            ((variable->min.set && variable->start < variable->min.value) ||
             (variable->max.set && variable->start > variable->max.value))) {
            fprintf(stderr, "error: start value of '%s' lies outside [min, max]\n", variable->name);
            problems++;
        }

        /* Only the initialization of a state can replace its start value */
        if (variable->initial == VAR_APPROX) {
            if (variable->role != VAR_STATE) {
                fprintf(stderr, "error: '%s' is not a state, only states can have .initial = VAR_APPROX\n",
                        variable->name);
                problems++;
            } else if (!def->initialize) {
                fprintf(stderr, "error: state '%s' has .initial = VAR_APPROX, but the model has no initialize "
                                "function that could replace its start value\n", variable->name);
                problems++;
            }
        }

        if (variable->role == VAR_CALCULATED_PARAMETER) {
            hasCalculatedParameters = true;
        }

        /* Enumeration variables */
        if (variable->declaredType && variable->type != VAR_INTEGER) {
            fprintf(stderr, "error: '%s' is not an int, only Integer variables can have a declaredType\n",
                    variable->name);
            problems++;
        } else if (variable->declaredType) {
            if (isFirstUseOfType(def, k)) {
                problems += validateEnumeration(def, k);
            }
            if (variable->min.set || variable->max.set) {
                fprintf(stderr, "error: enumeration variable '%s' has min or max; its values are the items "
                                "of its type\n", variable->name);
                problems++;
            }
            /* (a type without items is already reported by validateEnumeration) */
            if (hasStartValue(variable->role) && variable->declaredType->numberOfItems > 0 &&
                !modelEnumerationHasValue(variable->declaredType, (int)variable->start)) {
                fprintf(stderr, "error: start value %d of '%s' is not an item of its enumeration\n",
                        (int)variable->start, variable->name);
                problems++;
            }
        }

        /* String variables, and the start text that only they have */
        if (variable->type == VAR_STRING) {
            problems += validateString(variable);
        } else if (variable->startString) {
            fprintf(stderr, "error: '%s' has .startString, but only string parameters have a start text "
                            "(use .start)\n", variable->name);
            problems++;
        }

        /* Dependencies must be inputs of this model */
        if (variable->dependsOn) {
            problems += validateDependencies(def, variable);
        }
    }

    if (hasCalculatedParameters && !def->calculateParameters) {
        fprintf(stderr, "error: the model has calculated parameters but no calculateParameters function\n");
        problems++;
    }

    if (numberOfStateVariables != def->numberOfStates) {
        fprintf(stderr, "error: the struct States has %zu doubles but %zu states are declared "
                        "(every field of States must be declared with STATE and must be a double)\n",
                        def->numberOfStates, numberOfStateVariables);
        problems++;
    }

    return problems;
}

/***************************************************
 * XML Sections
 ***************************************************/

/**
 * Write the UnitDefinitions element
 *
 * Contains all units of the model's unit table, followed by units that
 * are used by a variable but not defined in the table (without BaseUnit).
 */
static void writeUnitDefinitions(FILE* file, const modelDefinition* def)
{
    bool opened = false;

    for (size_t k = 0; k < def->numberOfUnits; k++) {
        const modelUnit* unit = &def->units[k];

        if (!opened) {
            fputs("    <UnitDefinitions>\n", file);
            opened = true;
        }

        fputs("        <Unit", file);
        writeStringAttribute(file, "name", unit->name);
        fputs(">\n            <BaseUnit", file);
        writeExponent(file, "kg", unit->kg);
        writeExponent(file, "m", unit->m);
        writeExponent(file, "s", unit->s);
        writeExponent(file, "A", unit->A);
        writeExponent(file, "K", unit->K);
        writeExponent(file, "mol", unit->mol);
        writeExponent(file, "cd", unit->cd);
        writeExponent(file, "rad", unit->rad);
        fputs("/>\n        </Unit>\n", file);
    }

    for (size_t k = 0; k < def->numberOfVariables; k++) {
        const modelVariable* variable = &def->variables[k];

        if (variable->type != VAR_REAL || !variable->unit || unitIsDefined(def, variable->unit)) {
            continue;
        }

        /* Skip units already written by an earlier variable */
        bool alreadyWritten = false;
        for (size_t j = 0; j < k; j++) {
            const modelVariable* earlier = &def->variables[j];
            if (earlier->type == VAR_REAL && earlier->unit && strcmp(earlier->unit, variable->unit) == 0) {
                alreadyWritten = true;
            }
        }
        if (alreadyWritten) {
            continue;
        }

        if (!opened) {
            fputs("    <UnitDefinitions>\n", file);
            opened = true;
        }

        fputs("        <Unit", file);
        writeStringAttribute(file, "name", variable->unit);
        fputs("/>\n", file);
    }

    if (opened) {
        fputs("    </UnitDefinitions>\n\n", file);
    }
}

/**
 * Write the TypeDefinitions element
 *
 * Contains every enumeration type used by a variable, once, in the order of
 * the first variable that uses it.
 */
static void writeTypeDefinitions(FILE* file, const modelDefinition* def)
{
    bool opened = false;

    for (size_t k = 0; k < def->numberOfVariables; k++) {
        const modelEnumeration* type = def->variables[k].declaredType;

        if (!type || !isFirstUseOfType(def, k)) {
            continue;
        }

        if (!opened) {
            fputs("    <TypeDefinitions>\n", file);
            opened = true;
        }

        fputs("        <SimpleType", file);
        writeStringAttribute(file, "name", type->name);
        writeStringAttribute(file, "description", type->description);
        fputs(">\n            <Enumeration>\n", file);
        for (size_t i = 0; i < type->numberOfItems; i++) {
            const modelEnumerationItem* item = &type->items[i];
            fputs("                <Item", file);
            writeStringAttribute(file, "name", item->name);
            fprintf(file, " value=\"%d\"", item->value);
            writeStringAttribute(file, "description", item->description);
            fputs("/>\n", file);
        }
        fputs("            </Enumeration>\n        </SimpleType>\n", file);
    }

    if (opened) {
        fputs("    </TypeDefinitions>\n\n", file);
    }
}

/**
 * Write the DefaultExperiment element with the settings the model suggests
 *
 * Every attribute of DefaultExperiment is optional (FMI 2.0, section 2.2.5,
 * Definition of a Default Experiment), so each setting is written on its
 * own: the time window if the stop time lies behind the start time, the
 * tolerance and the step size if they are greater than 0. A model that
 * suggests nothing gets no element. So a model that only suggests a
 * tolerance still publishes it; it is also the tolerance the FMU uses when
 * the integration tool defines none (see modelDefaultTolerance()).
 */
static void writeDefaultExperiment(FILE* file, const modelExperiment* experiment)
{
    bool hasTimeWindow = experiment->stopTime > experiment->startTime;
    bool hasTolerance  = experiment->tolerance > 0.0;
    bool hasStepSize   = experiment->stepSize > 0.0;

    if (!hasTimeWindow && !hasTolerance && !hasStepSize) {
        return;
    }

    fputs("    <DefaultExperiment", file);
    if (hasTimeWindow) {
        writeRealAttribute(file, "startTime", experiment->startTime);
        writeRealAttribute(file, "stopTime", experiment->stopTime);
    }
    if (hasTolerance) {
        writeRealAttribute(file, "tolerance", experiment->tolerance);
    }
    if (hasStepSize) {
        writeRealAttribute(file, "stepSize", experiment->stepSize);
    }
    fputs("/>\n\n", file);
}

/**
 * Title of the group of variables with the given role
 */
static const char* roleTitle(varRole role)
{
    switch (role) {
        case VAR_PARAMETER: return "Parameters";
        case VAR_INPUT:     return "Inputs";
        case VAR_OUTPUT:    return "Outputs";
        case VAR_STATE:     return "States";
        case VAR_LOCAL:     return "Locals";
        case VAR_CALCULATED_PARAMETER: return "Calculated parameters";
    }
    return "";
}

/**
 * Write one ScalarVariable element
 *
 * Mapping of the variable's role to FMI attributes:
 *
 *   role        causality            variability           initial       start
 *   parameter   parameter            tunable / fixed       exact         yes
 *   calculated  calculatedParameter  tunable / fixed       calculated    no
 *   parameter
 *   input       input                continuous / discrete -             yes
 *   output      output               continuous / discrete calculated    no
 *   state       local                continuous            exact/approx  yes
 *   local       local                continuous / discrete calculated    no
 *
 * Strings are parameters only (see validateDefinition()), published as
 * <String start="..."/> with the start text ("" if the model gives none:
 * a parameter has initial="exact", which needs a start value).
 *
 * Real inputs, outputs and locals are continuous, Integer and Boolean ones
 * are discrete (FMI 2.0 only allows continuous for Real variables). States
 * and local variables share causality "local"; initial tells them apart: a
 * state can be set before the simulation (exact; approx only before the
 * initialization, which may replace it), a local variable is computed by
 * the model (calculated).
 */
static void writeScalarVariable(FILE* file, const modelVariable* variable, size_t valueReference)
{
    const char* causality = "";
    const char* variability = (variable->type == VAR_REAL) ? "continuous" : "discrete";
    const char* initial = NULL;
    bool hasStart = true;

    switch (variable->role) {
        case VAR_PARAMETER:
            causality = "parameter";
            variability = (variable->variability == VAR_FIXED) ? "fixed" : "tunable";
            initial = "exact";
            break;
        case VAR_INPUT:
            causality = "input";
            break;
        case VAR_OUTPUT:
            causality = "output";
            initial = "calculated";
            hasStart = false;
            break;
        case VAR_STATE:
            causality = "local";
            initial = (variable->initial == VAR_APPROX) ? "approx" : "exact";
            break;
        case VAR_LOCAL:
            causality = "local";
            initial = "calculated";
            hasStart = false;
            break;
        case VAR_CALCULATED_PARAMETER:
            causality = "calculatedParameter";
            variability = (variable->variability == VAR_FIXED) ? "fixed" : "tunable";
            initial = "calculated";
            hasStart = false;
            break;
    }

    fputs("        <ScalarVariable", file);
    writeStringAttribute(file, "name", variable->name);
    fprintf(file, " valueReference=\"%zu\"", valueReference);
    writeStringAttribute(file, "description", variable->description);
    writeStringAttribute(file, "causality", causality);
    writeStringAttribute(file, "variability", variability);
    writeStringAttribute(file, "initial", initial);
    fputs(">\n", file);

    switch (variable->type) {
        case VAR_REAL:
            fputs("            <Real", file);
            if (hasStart) {
                writeRealAttribute(file, "start", variable->start);
            }
            writeStringAttribute(file, "unit", variable->unit);
            if (variable->min.set) {
                writeRealAttribute(file, "min", variable->min.value);
            }
            if (variable->max.set) {
                writeRealAttribute(file, "max", variable->max.value);
            }
            if (variable->nominal != 0.0) {
                writeRealAttribute(file, "nominal", variable->nominal);
            }
            break;
        case VAR_INTEGER:
            if (variable->declaredType) {
                /* Enumeration: the bounds are the items of the type */
                fputs("            <Enumeration", file);
                writeStringAttribute(file, "declaredType", variable->declaredType->name);
                if (hasStart) {
                    fprintf(file, " start=\"%d\"", (int)variable->start);
                }
                break;
            }
            fputs("            <Integer", file);
            if (hasStart) {
                fprintf(file, " start=\"%d\"", (int)variable->start);
            }
            if (variable->min.set) {
                fprintf(file, " min=\"%d\"", (int)variable->min.value);
            }
            if (variable->max.set) {
                fprintf(file, " max=\"%d\"", (int)variable->max.value);
            }
            break;
        case VAR_BOOLEAN:
            fputs("            <Boolean", file);
            if (hasStart) {
                fprintf(file, " start=\"%s\"", (variable->start != 0.0) ? "true" : "false");
            }
            break;
        case VAR_STRING:
            fputs("            <String", file);
            if (hasStart) {
                writeStringAttribute(file, "start", variable->startString ? variable->startString : "");
            }
            break;
    }

    fputs("/>\n        </ScalarVariable>\n", file);
}

/**
 * Write a list of Unknown elements, one per variable whose role is
 * included (outputs in <Outputs>; outputs and calculated parameters in
 * <InitialUnknowns>)
 *
 * The index refers to the position in ModelVariables (1-based), so the list
 * is in ascending order as FMI 2.0 requires.
 *
 * With dependencies, the inputs an output depends on directly are written
 * as declared by the model (dependsOn). An output that declares nothing
 * gets no dependencies attribute, which means "depends on all knowns".
 */
static void writeUnknowns(FILE* file, const modelDefinition* def, const char* element,
                          bool withCalculatedParameters, bool withDependencies)
{
    bool opened = false;

    for (size_t k = 0; k < def->numberOfVariables; k++) {
        varRole role = def->variables[k].role;
        if (role != VAR_OUTPUT && !(withCalculatedParameters && role == VAR_CALCULATED_PARAMETER)) {
            continue;
        }
        if (!opened) {
            fprintf(file, "        <%s>\n", element);
            opened = true;
        }
        fprintf(file, "            <Unknown index=\"%zu\"", k + 1);

        /* Indices of the inputs named in dependsOn, in ascending order */
        const char* dependsOn = def->variables[k].dependsOn;
        if (withDependencies && dependsOn) {
            const char* separator = "";

            fputs(" dependencies=\"", file);
            for (size_t j = 0; j < def->numberOfVariables; j++) {
                const modelVariable* other = &def->variables[j];
                if (other->role == VAR_INPUT && nameListContains(dependsOn, other->name)) {
                    fprintf(file, "%s%zu", separator, j + 1);
                    separator = " ";
                }
            }
            fputs("\"", file);
        }

        fputs("/>\n", file);
    }

    if (opened) {
        fprintf(file, "        </%s>\n", element);
    }
}

/**
 * Write the complete modelDescription.xml
 */
static void writeModelDescription(FILE* file, const modelDefinition* def)
{
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", file);
    fputs("<!-- Generated by csfmu from the model definition. Do not edit. -->\n", file);

    /* Root element */
    fputs("<fmiModelDescription fmiVersion=\"2.0\"", file);
    writeStringAttribute(file, "modelName", def->name);
    writeStringAttribute(file, "guid", def->guid);
    writeStringAttribute(file, "description", def->description);
    writeStringAttribute(file, "version", def->version);
    fputs(" generationTool=\"csfmu\" numberOfEventIndicators=\"0\">\n\n", file);

    /* Co-Simulation capabilities */
    fputs("    <CoSimulation", file);
    writeStringAttribute(file, "modelIdentifier", def->name);
    fputs(" needsExecutionTool=\"false\""
          " canHandleVariableCommunicationStepSize=\"" CAN_HANDLE_VARIABLE_COMMUNICATION_STEP_SIZE "\""
          " canInterpolateInputs=\"" CAN_INTERPOLATE_INPUTS "\""
          " maxOutputDerivativeOrder=\"" MAX_OUTPUT_DERIVATIVE_ORDER "\""
          " canGetAndSetFMUstate=\"" CAN_GET_AND_SET_FMU_STATE "\""
          " canSerializeFMUstate=\"" CAN_SERIALIZE_FMU_STATE "\""
          " providesDirectionalDerivative=\"" PROVIDES_DIRECTIONAL_DERIVATIVE "\""
          " canBeInstantiatedOnlyOncePerProcess=\"false\""
          " canNotUseMemoryManagementFunctions=\"" CAN_NOT_USE_MEMORY_MANAGEMENT_FUNCTIONS "\"/>\n\n", file);

    /* Unit and type definitions */
    writeUnitDefinitions(file, def);
    writeTypeDefinitions(file, def);

    /* Log categories (must match LOG_CATEGORIES in fmi2Functions.c) */
    fputs("    <LogCategories>\n"
          "        <Category name=\"logAll\" description=\"All log messages\"/>\n"
          "        <Category name=\"logEvents\" description=\"Lifecycle of the FMU\"/>\n"
          "        <Category name=\"logStatusWarning\" description=\"A function returned fmi2Warning or fmi2Discard\"/>\n"
          "        <Category name=\"logStatusError\" description=\"A function returned fmi2Error\"/>\n"
          "        <Category name=\"logFmi2Call\" description=\"Successful calls of fmi2GetXXX, fmi2SetXXX and fmi2DoStep\"/>\n"
          "    </LogCategories>\n\n", file);

    /* Default experiment (the settings the model suggests, if any) */
    writeDefaultExperiment(file, &def->defaultExperiment);

    /* Model variables (value reference = index in the variable table).
       The variables keep the order of the table; a comment marks every
       place where the role changes. */
    fputs("    <ModelVariables>\n", file);
    for (size_t k = 0; k < def->numberOfVariables; k++) {
        varRole role = def->variables[k].role;
        if (k == 0) {
            fprintf(file, "        <!-- %s -->\n", roleTitle(role));
        } else if (role != def->variables[k - 1].role) {
            fprintf(file, "\n        <!-- %s -->\n", roleTitle(role));
        }
        writeScalarVariable(file, &def->variables[k], k);
    }
    fputs("    </ModelVariables>\n\n", file);

    /* Model structure (the initial unknowns also depend on parameters and
       states, which dependsOn does not describe: "depends on all knowns").
       FMI 2.0, section 2.2.8 (ModelStructure) lists outputs in <Outputs>, and
       outputs, calculated parameters and (for Model Exchange) states and
       derivatives in <InitialUnknowns>. Local variables are not listed */
    fputs("    <ModelStructure>\n", file);
    writeUnknowns(file, def, "Outputs", false, true);
    writeUnknowns(file, def, "InitialUnknowns", true, false);
    fputs("    </ModelStructure>\n\n", file);

    fputs("</fmiModelDescription>\n", file);
}

/***************************************************
 * Entry Point
 ***************************************************/

int main(int argc, char* argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <output file> <model identifier>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const modelDefinition* def = &MODEL_DEFINITION;

    int problems = validateDefinition(def, argv[2]);
    if (problems > 0) {
        fprintf(stderr, "modelDescription.xml was not generated: %d problem(s) in the model definition\n",
                problems);
        return EXIT_FAILURE;
    }

    FILE* file = fopen(argv[1], "w");
    if (!file) {
        fprintf(stderr, "error: cannot open '%s' for writing\n", argv[1]);
        return EXIT_FAILURE;
    }

    writeModelDescription(file, def);

    if (fclose(file) != 0) {
        fprintf(stderr, "error: cannot write '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
