#ifndef FMI3ADAPTER_H
#define FMI3ADAPTER_H

#include "fmi3Functions.h"
#include "model.h"

/**
 * An instance of the FMU3
 */
typedef struct {
    fmi3String instanceName;                      ///> Name of the FMU
    fmi3String instantiationToken;                ///> ID to match the modelDescription.xml
    
    fmi3LogMessageCallback logMessage;            ///> logging function
    fmi3InstanceEnvironment instanceEnvironment;  ///> ?
    
    modelInstance* model;                         ///> Instance of the model
    
} myFMU3;


#endif