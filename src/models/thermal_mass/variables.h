#ifndef THERMAL_MASS_VARIABLES_H
#define THERMAL_MASS_VARIABLES_H

#include "model.h"

/* initialization modes (parameter init_mode) */
#define THERMAL_MASS_FIXED_START  1  /* T starts at its start value */
#define THERMAL_MASS_STEADY_STATE 2  /* T starts in equilibrium: T = T_amb + R*P */

/* parameters */
typedef struct {
    double C;          /* heat capacity */
    double R;          /* thermal resistance to the ambient */
    double T_amb;      /* ambient temperature */
    int    init_mode;  /* how T is initialized (THERMAL_MASS_FIXED_START, ...) */
    double tau;        /* time constant R*C (calculated parameter) */
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
/* calculated parameters */
void thermalMassParameters(modelInstance* model);
/* initialization */
simStatus thermalMassInitialize(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
