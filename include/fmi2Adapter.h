#ifndef FMI2ADAPTER_H
#define FMI2ADAPTER_h

#include "fmi2Functions.h"
#include "model.h"

/**
 * An instance of the FMU2
 */
typedef struct {

    modelInstance* model; ///> Instance of the model
} myFMU2;

#endif