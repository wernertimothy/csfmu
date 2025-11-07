
#include "fmi3Functions.h"
#include "config.h"
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

/**
 * Get FMI Version
 */
const char* fmi3GetVersion(void) {
    return "3";
} 

/**
 * Set Debug Logging
 */
fmi3Status fmi3SetDebugLogging(fmi3Instance instance,
                               fmi3Boolean loggingOn,
                               size_t nCategories,
                               const fmi3String categories[])
{
    // TODO: Implement
    return fmi3OK;
}

/**
 * Instantiate Co Simualtion
 */
fmi3Instance fmi3InstantiateCoSimulation(
    fmi3String                     instanceName,
    fmi3String                     instantiationToken,
    fmi3String                     resourcePath,
    fmi3Boolean                    visible,
    fmi3Boolean                    loggingOn,
    fmi3Boolean                    eventModeUsed,
    fmi3Boolean                    earlyReturnAllowed,
    const fmi3ValueReference       requiredIntermediateVariables[],
    size_t                         nRequiredIntermediateVariables,
    fmi3InstanceEnvironment        instanceEnvironment,
    fmi3LogMessageCallback         logMessage,
    fmi3IntermediateUpdateCallback intermediateUpdate)
{
    // Create an fmi3 instanace
    myFMU3* myfmu3 = malloc(sizeof(myFMU3));
    if (!myfmu3) {
        return NULL;  // Memory allocation failed
    }
    
    myfmu3->instanceName        = instanceName;
    myfmu3->instantiationToken  = instantiationToken;
    myfmu3->logMessage          = logMessage;
    myfmu3->instanceEnvironment = instanceEnvironment;
    
    // Create a model instance
    myfmu3->model = newModel(NUMBER_OF_STATES, NUMBER_OF_INPUTS);
    if (!myfmu3->model) {
        freeModel(myfmu3->model);
        fmi3FreeInstance(myfmu3);
        return NULL;
    }
    // initialize model
    myfmu3->model->parameter->k   = 1.0;
    myfmu3->model->parameter->tau = 0.15;
    myfmu3->model->states[0]      = 0.0;
    myfmu3->model->getStateDerivatives = MODEL_STATE_EQUATION;
    myfmu3->model->integrate           = INTEGRATOR_METHOD;
    
    // Log a message to confirm successful instantiation
    // if (logMessage != NULL && loggingOn) {
    //     logMessage(instanceEnvironment, myfmu3->instanceName, fmi3OK, "logAll", "FMU instantiated successfully");
    // }
    
    return (fmi3Instance)myfmu3;
}

/**
 * Free FMU3 instance
 */
void fmi3FreeInstance(fmi3Instance instance)
{
    free(instance);
}