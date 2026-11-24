#include <stddef.h>
#include "variables.h"

/*
 * The table: points (u, y) with strictly increasing u. The string parameter
 * table_file is the name of the table file; the points are built in.
 */
static const double TABLE_U[] = { -1.0, 0.0, 1.0, 2.0, 3.0 };
static const double TABLE_Y[] = { -2.0, 0.0, 1.0, 1.5, 1.75 };

#define TABLE_POINTS (sizeof(TABLE_U) / sizeof(TABLE_U[0]))

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
 */
void lookupTable1dOutputs(modelInstance* model)
{
    const Inputs* in  = model->inputs;
    Outputs*      out = model->outputs;
    double        u   = in->u;

    if (u <= TABLE_U[0]) {
        out->y = TABLE_Y[0];
        return;
    }
    if (u >= TABLE_U[TABLE_POINTS - 1]) {
        out->y = TABLE_Y[TABLE_POINTS - 1];
        return;
    }

    /* segment k with TABLE_U[k] <= u < TABLE_U[k + 1] */
    size_t k = 0;
    while (u >= TABLE_U[k + 1]) {
        k++;
    }

    double fraction = (u - TABLE_U[k]) / (TABLE_U[k + 1] - TABLE_U[k]);
    out->y = TABLE_Y[k] + (TABLE_Y[k + 1] - TABLE_Y[k]) * fraction;
}
