#ifndef LOOKUP_TABLE_1D_VARIABLES_H
#define LOOKUP_TABLE_1D_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    char table_file[MODEL_STRING_SIZE];  /* name of the table file: a string parameter */
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

#ifdef __cplusplus
extern "C" {
#endif

/* state equation (nothing to compute, the model has no states) */
void lookupTable1dSystem(modelInstance* model);
/* output equation */
void lookupTable1dOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
