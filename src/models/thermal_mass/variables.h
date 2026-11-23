#ifndef THERMAL_MASS_VARIABLES_H
#define THERMAL_MASS_VARIABLES_H

#include "model.h"

/* parameters */
typedef struct {
    double C;      /* heat capacity */
    double R;      /* thermal resistance to the ambient */
    double T_amb;  /* ambient temperature */
} Params;

/* inputs */
typedef struct {
    double P;      /* heating power */
} Inputs;

/* outputs */
typedef struct {
    double T_out;  /* temperature of the mass */
} Outputs;

/* local variables (published, read-only) */
typedef struct {
    double Q_loss; /* heat flow from the mass to the ambient */
} Locals;

/* continuous states (doubles only) */
typedef struct {
    double T;      /* temperature of the mass */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void thermalMassSystem(modelInstance* model);
/* output equation (outputs and local variables) */
void thermalMassOutputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
