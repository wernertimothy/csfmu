#ifndef LOOKUP_TABLE_1D_VARIABLES_H
#define LOOKUP_TABLE_1D_VARIABLES_H

#include "model.h"

/* largest number of points of the table */
#define LOOKUP_TABLE_1D_MAX_POINTS 256

/* parameters */
typedef struct {
    char table_file[MODEL_STRING_SIZE];  /* name of the table file in the resources: a string parameter */
} Params;

/* inputs */
typedef struct {
    double u;  /* value looked up in the table */
} Inputs;

/* outputs */
typedef struct {
    double y;  /* interpolated table value */
} Outputs;

/* no continuous states: the model is a static map y = h(u) */

/* internals (not published): the table, read from the table file by the initialization */
typedef struct {
    int    numberOfPoints;                  /* 0 until the table is read */
    double u[LOOKUP_TABLE_1D_MAX_POINTS];   /* strictly increasing */
    double y[LOOKUP_TABLE_1D_MAX_POINTS];
} Internals;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation (nothing to compute, the model has no states) */
void lookupTable1dSystem(modelInstance* model);

/* output equation */
void lookupTable1dOutputs(modelInstance* model);

/* initialization: reads the table file */
simStatus lookupTable1dInitialize(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
