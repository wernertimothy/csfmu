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
 * - The capability flags and log categories describe fmi2Functions.c and
 *   are therefore defined here, not in the model.
 *
 * Usage: generate_model_description <output file>
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
#define CAN_GET_AND_SET_FMU_STATE                   "false"
#define CAN_SERIALIZE_FMU_STATE                     "false"
#define PROVIDES_DIRECTIONAL_DERIVATIVE             "false"

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
    }
    return 0;
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

/***************************************************
 * Validation
 ***************************************************/

/**
 * Check the model definition for mistakes that the compiler cannot catch
 *
 * Prints one message per problem and returns the number of problems.
 *
 * Checks:
 * - Required fields of the definition are set
 * - Variable names are present and unique
 * - Every variable lies inside its storage block
 * - The states cover the state vector exactly (every double of the struct
 *   States is published once)
 * - Start values lie within [min, max]
 */
static int validateDefinition(const modelDefinition* def)
{
    int problems = 0;

    if (!def->name || !def->guid) {
        fprintf(stderr, "error: model definition needs a name and a guid\n");
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
                                "(use PARAM_AS, INPUT_AS, OUTPUT_AS or STATE_AS to publish "
                                "a field under a different name)\n", variable->name);
                problems++;
            }
        }

        /* Variable must lie inside its storage block */
        if (variable->offset + sizeOfType(variable->type) > sizeOfBlock(def, variable->role)) {
            fprintf(stderr, "error: variable '%s' lies outside its storage block "
                            "(check sizeOfParams, sizeOfInputs, sizeOfOutputs, numberOfStates)\n",
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

        /* Start value must respect the bounds */
        if ((variable->min.set && variable->start < variable->min.value) ||
            (variable->max.set && variable->start > variable->max.value)) {
            fprintf(stderr, "error: start value of '%s' lies outside [min, max]\n", variable->name);
            problems++;
        }
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
 * Write one ScalarVariable element
 *
 * Mapping of the variable's role to FMI attributes:
 *
 *   role       causality   variability           initial      start
 *   parameter  parameter   tunable / fixed       exact        yes
 *   input      input       continuous / discrete -            yes
 *   output     output      continuous / discrete calculated   no
 *   state      local       continuous            exact        yes
 *
 * Real inputs and outputs are continuous, Integer and Boolean ones are
 * discrete (FMI 2.0 only allows continuous for Real variables).
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
            initial = "exact";
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
    }

    fputs("/>\n        </ScalarVariable>\n", file);
}

/**
 * Write a list of Unknown elements, one per output
 *
 * The index refers to the position in ModelVariables (1-based). No
 * dependencies attribute is written, which means "depends on all knowns".
 */
static void writeOutputUnknowns(FILE* file, const modelDefinition* def, const char* element)
{
    bool opened = false;

    for (size_t k = 0; k < def->numberOfVariables; k++) {
        if (def->variables[k].role != VAR_OUTPUT) {
            continue;
        }
        if (!opened) {
            fprintf(file, "        <%s>\n", element);
            opened = true;
        }
        fprintf(file, "            <Unknown index=\"%zu\"/>\n", k + 1);
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
    fputs("<!-- Generated by fmuTemplate from the model definition. Do not edit. -->\n", file);

    /* Root element */
    fputs("<fmiModelDescription fmiVersion=\"2.0\"", file);
    writeStringAttribute(file, "modelName", def->name);
    writeStringAttribute(file, "guid", def->guid);
    writeStringAttribute(file, "description", def->description);
    writeStringAttribute(file, "version", def->version);
    fputs(" generationTool=\"fmuTemplate\" numberOfEventIndicators=\"0\">\n\n", file);

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
          " canNotUseMemoryManagementFunctions=\"false\"/>\n\n", file);

    /* Unit definitions */
    writeUnitDefinitions(file, def);

    /* Log categories (match the categories handled by fmi2SetDebugLogging) */
    fputs("    <LogCategories>\n"
          "        <Category name=\"logAll\" description=\"All log messages\"/>\n"
          "        <Category name=\"logEvents\" description=\"Event messages\"/>\n"
          "        <Category name=\"logStatusWarning\" description=\"Warning messages\"/>\n"
          "        <Category name=\"logStatusError\" description=\"Error messages\"/>\n"
          "    </LogCategories>\n\n", file);

    /* Default experiment (only if the model suggests one) */
    const modelExperiment* experiment = &def->defaultExperiment;
    if (experiment->stopTime > experiment->startTime) {
        fputs("    <DefaultExperiment", file);
        writeRealAttribute(file, "startTime", experiment->startTime);
        writeRealAttribute(file, "stopTime", experiment->stopTime);
        if (experiment->tolerance > 0.0) {
            writeRealAttribute(file, "tolerance", experiment->tolerance);
        }
        if (experiment->stepSize > 0.0) {
            writeRealAttribute(file, "stepSize", experiment->stepSize);
        }
        fputs("/>\n\n", file);
    }

    /* Model variables (value reference = index in the variable table) */
    fputs("    <ModelVariables>\n", file);
    for (size_t k = 0; k < def->numberOfVariables; k++) {
        writeScalarVariable(file, &def->variables[k], k);
    }
    fputs("    </ModelVariables>\n\n", file);

    /* Model structure */
    fputs("    <ModelStructure>\n", file);
    writeOutputUnknowns(file, def, "Outputs");
    writeOutputUnknowns(file, def, "InitialUnknowns");
    fputs("    </ModelStructure>\n\n", file);

    fputs("</fmiModelDescription>\n", file);
}

/***************************************************
 * Entry Point
 ***************************************************/

int main(int argc, char* argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const modelDefinition* def = &MODEL_DEFINITION;

    int problems = validateDefinition(def);
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
