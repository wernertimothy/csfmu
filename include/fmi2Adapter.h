#ifndef FMI2ADAPTER_H
#define FMI2ADAPTER_H

#include "fmi2Functions.h"
#include "model.h"

/**
 * An instance of the FMU2
 */
typedef struct {
    // TODO : sort by size to minimize padding
    fmi2String instanceName; ///> Name of the FMU
    fmi2Type fmuType; ///> CS of ME

    modelInstance* model; ///> Instance of the model
} myFMU2;

#endif