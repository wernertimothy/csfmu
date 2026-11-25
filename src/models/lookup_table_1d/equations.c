#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "variables.h"

/* Longest line of the table file, including the line break and the '\0' */
#define LINE_SIZE 256

/* Longest number of the table file, in characters */
#define NUMBER_SIZE 64

/**
 * Lookup table state equation
 *
 * The model has no states, so there is nothing to compute. The FMU still
 * calls the function once per integration step.
 */
void lookupTable1dSystem(modelInstance* model)
{
    (void)model;
}

/**
 * Lookup table output equation: y = interp1(table, u)
 *
 * Linear interpolation between the two points around u. Outside the table
 * the output is clamped to the first or last value:
 *
 *     u <= u_first:  y = y_first
 *     u >= u_last:   y = y_last
 *     otherwise:     y = y_k + (y_{k+1} - y_k) * (u - u_k) / (u_{k+1} - u_k),
 *                    with u_k <= u < u_{k+1}
 *
 * Implementation notes:
 * - Direct feedthrough of u, hence .dependsOn = "u" in variables.c: the
 *   FMU recomputes y as soon as u is set, without a step
 * - The table is read by the initialization. Before (and after fmi2Reset,
 *   until the next initialization) there is no table and y is NaN: it
 *   cannot be computed yet
 */
void lookupTable1dOutputs(modelInstance* model)
{
    const Internals* table = model->internals;
    const Inputs*    in    = model->inputs;
    Outputs*         out   = model->outputs;
    double           u     = in->u;
    int              last  = table->numberOfPoints - 1;

    if (table->numberOfPoints == 0) {
        out->y = NAN;
        return;
    }
    if (u <= table->u[0]) {
        out->y = table->y[0];
        return;
    }
    if (u >= table->u[last]) {
        out->y = table->y[last];
        return;
    }

    /* segment k with u[k] <= u < u[k + 1] (a NaN input stays in segment 0 and gives NaN) */
    int k = 0;
    while (k < last - 1 && u >= table->u[k + 1]) {
        k++;
    }

    double fraction = (u - table->u[k]) / (table->u[k + 1] - table->u[k]);
    out->y = table->y[k] + (table->y[k + 1] - table->y[k]) * fraction;
}

/**
 * Position of the first character that is no space or tab
 */
static const char* skipBlanks(const char* c)
{
    while (*c == ' ' || *c == '\t') {
        c++;
    }
    return c;
}

/**
 * Read a number at position (after optional blanks)
 *
 * The table file always uses '.' as decimal point. strtod() follows the
 * locale of the C library, which an integration tool may have changed
 * (e.g. to a German locale with ','), so the number is copied with '.'
 * replaced by the decimal point of the current locale first. Only digits,
 * signs, '.' and exponents belong to a number, so "inf", "nan" and
 * hexadecimal numbers are no numbers here.
 *
 * Returns the position behind the number, or NULL if there is none.
 */
static const char* parseNumber(const char* position, double* value)
{
    const char* c = skipBlanks(position);
    char decimalPoint = localeconv()->decimal_point[0];
    char number[NUMBER_SIZE];
    size_t length = 0;

    while ((*c >= '0' && *c <= '9') || *c == '+' || *c == '-' || *c == '.' || *c == 'e' || *c == 'E') {
        if (length + 1 >= sizeof(number)) {
            return NULL;
        }
        number[length++] = (*c == '.') ? decimalPoint : *c;
        c++;
    }
    number[length] = '\0';

    char* end = NULL;
    *value = strtod(number, &end);
    if (length == 0 || end != number + length) {
        return NULL;  /* nothing, or not all of it is a number (e.g. "1-2") */
    }
    return c;
}

/**
 * Read a point "u,y" from a line without line break
 *
 * Returns 1 for a point, 0 for an empty line and -1 for anything else.
 */
static int parsePoint(const char* line, double* u, double* y)
{
    if (*skipBlanks(line) == '\0') {
        return 0;
    }

    const char* c = parseNumber(line, u);
    if (!c || *skipBlanks(c) != ',') {
        return -1;
    }
    c = parseNumber(skipBlanks(c) + 1, y);
    if (!c || *skipBlanks(c) != '\0') {
        return -1;
    }
    return 1;
}

/**
 * Read the next line of the file into line, without its line break (\n or \r\n)
 *
 * Returns 1 for a line, 0 at the end of the file and -1 for a line that is
 * longer than LINE_SIZE - 1 characters.
 */
static int readLine(FILE* file, char line[LINE_SIZE])
{
    if (!fgets(line, LINE_SIZE, file)) {
        return 0;
    }

    size_t length = strlen(line);
    if (length > 0 && line[length - 1] == '\n') {
        line[--length] = '\0';
    } else if (!feof(file)) {
        return -1;
    }
    if (length > 0 && line[length - 1] == '\r') {
        line[--length] = '\0';
    }
    return 1;
}

/**
 * Read the table from the file at path into the internals
 *
 * Format: a header line (e.g. "u,y"), then one point "u,y" per line, u
 * strictly increasing. Blank lines are ignored, blanks around the numbers
 * and Windows line breaks (\r\n) are allowed. Every problem is reported
 * with the path of the file and, if it belongs to a line, the line number.
 * The table is only taken if the whole file is valid.
 */
static simStatus readTable(modelInstance* model, const char* path)
{
    Internals* table = model->internals;
    char line[LINE_SIZE];
    int lineNumber = 0;
    int points = 0;
    simStatus status = SIM_OK;

    FILE* file = modelOpenFile(path, "rb");
    if (!file) {
        return modelError(model, "Cannot open the table file '%s'.", path);
    }

    for (;;) {
        int read = readLine(file, line);
        if (read == 0) {
            break;
        }
        lineNumber++;
        if (read < 0) {
            status = modelError(model, "Line %d of the table file '%s' is longer than %d characters.",
                                lineNumber, path, LINE_SIZE - 2);
            break;
        }

        double u = 0.0;
        double y = 0.0;
        int kind = parsePoint(line, &u, &y);

        /* the first line is the header (a point there is a forgotten header) */
        if (lineNumber == 1) {
            if (kind > 0) {
                status = modelError(model, "The first line of the table file '%s' must be a header such as "
                                           "\"u,y\", not a point.", path);
                break;
            }
            continue;
        }

        if (kind == 0) {
            continue;
        }
        if (kind < 0) {
            status = modelError(model, "Line %d of the table file '%s' is not a point \"u,y\" of two numbers "
                                       "(with '.' as decimal point).", lineNumber, path);
            break;
        }
        if (!isfinite(u) || !isfinite(y)) {
            status = modelError(model, "Line %d of the table file '%s' has a value that is not finite.",
                                lineNumber, path);
            break;
        }
        if (points > 0 && !(u > table->u[points - 1])) {
            status = modelError(model, "Line %d of the table file '%s': u = %g is not greater than u = %g of "
                                       "the point before (u must be strictly increasing).",
                                lineNumber, path, u, table->u[points - 1]);
            break;
        }
        if (points == LOOKUP_TABLE_1D_MAX_POINTS) {
            status = modelError(model, "The table file '%s' has more than %d points.",
                                path, LOOKUP_TABLE_1D_MAX_POINTS);
            break;
        }

        table->u[points] = u;
        table->y[points] = y;
        points++;
    }

    if (status == SIM_OK && ferror(file)) {
        status = modelError(model, "Cannot read the table file '%s'.", path);
    }
    if (status == SIM_OK && points < 2) {
        status = modelError(model, "The table file '%s' has %d point(s), at least 2 are needed.", path, points);
    }
    fclose(file);

    table->numberOfPoints = (status == SIM_OK) ? points : 0;
    return status;
}

/**
 * Initialization: reads the table from the file table_file in the
 * resources of the FMU
 *
 * Implementation notes:
 * - model->resourcePath is the folder resources/ of the unzipped FMU and
 *   ends with '/', so the file name is simply appended. It is NULL if the
 *   integration tool did not give the location as a file URI
 * - The file is opened with modelOpenFile(), which also opens a path with
 *   non-ASCII characters on Windows
 * - Memory for the path is allocated here, not during a step
 * - The table is cached in the internals together with the name of its
 *   file. In initialization mode the FMU initializes the model whenever a
 *   variable is read after a set, e.g. while the integration tool iterates
 *   u, and the file is only read again for another table_file (or after
 *   fmi2Reset, which zeroes the internals)
 * - loadedFile is cleared and copied as a whole field (table_file has zero
 *   bytes behind its text), so equal model data also have equal bytes
 */
simStatus lookupTable1dInitialize(modelInstance* model)
{
    const Params* p = model->params;
    Internals* table = model->internals;

    if (table->numberOfPoints > 0 && strcmp(table->loadedFile, p->table_file) == 0) {
        return SIM_OK;
    }

    table->numberOfPoints = 0;
    memset(table->loadedFile, 0, sizeof(table->loadedFile));

    if (!model->resourcePath) {
        return modelError(model, "The FMU has no resource folder, so the table file '%s' cannot be read "
                                 "(fmuResourceLocation was no file URI).", p->table_file);
    }
    if (p->table_file[0] == '\0') {
        return modelError(model, "table_file is empty: it must name a file in the resources of the FMU.");
    }

    size_t size = strlen(model->resourcePath) + strlen(p->table_file) + 1;
    char* path = malloc(size);
    if (!path) {
        return modelError(model, "Memory allocation failed.");
    }
    snprintf(path, size, "%s%s", model->resourcePath, p->table_file);

    simStatus status = readTable(model, path);
    free(path);
    if (status == SIM_OK) {
        memcpy(table->loadedFile, p->table_file, sizeof(table->loadedFile));
    }
    return status;
}
