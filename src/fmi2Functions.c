
#include <string.h>
#include "fmi2Functions.h"
#include "model_config.h"
#include "model.h"

#define EPSILON 1e-9 // numerical significance boarder

/**
 * An instance of the FMU2
 */
typedef struct {
    // TODO : sort by size to minimize padding
    /* meta data */
    fmi2String instanceName; ///> Name of the FMU
    fmi2Type fmuType;        ///> CS or ME

    /* experiment */
    fmi2Real startTime; ///> start time of experiment
    fmi2Real stopTime;  ///> stop time of experiment
    fmi2Real time;      ///> internal clock

    /* logging */
    unsigned int logLevel; ///> bitmask of current log level

    /* fmu state */
    unsigned int state; ///> tracks the current state of the fmu

    /* functions */
    fmi2CallbackLogger         logger;               ///> Logger callback
    fmi2CallbackAllocateMemory allocateMemory;       ///> allocate memory callback
    fmi2CallbackFreeMemory     freeMemory;           ///> free memory callback
    fmi2StepFinished           stepFinished;         ///> asynchronous co-simualtion
    fmi2ComponentEnvironment   componentEnvironment; ///> integration tool context

    /* model */
    modelInstance* model; ///> Instance of the model
} myFMU2;

/**
 * logCategories enum
 */
typedef enum {
    logNone                  = 0,
    logEvents                = 1 << 0,
    logSingularLinearSystems = 1 << 1,
    logNonlinearSystems      = 1 << 2,
    logDynamicStateSelection = 1 << 3,
    logStatusWarning         = 1 << 4,
    logStatusDiscard         = 1 << 5,
    logStatusError           = 1 << 6,
    logStatusFatal           = 1 << 7,
    logStatusPending         = 1 << 8,
    logAll                   = (1 << 9) - 1 
} logCategory;

/**
 * fmu state enum
 */
typedef enum {
    stateInstantiated       = 1 << 0, // set by fmi2Instantiate
    stateInitializationMode = 1 << 1, // set by fmi2EnterInitializationMode
    stateInitialized        = 1 << 2, // set by fmi2ExitInitializationMode
} stateCategory;

/**
 * helper function to check log category
 */
static inline fmi2Boolean logCategoryActive(myFMU2* fmu, logCategory category) {
    return (fmu->logLevel & category);
}

/**
 * helper function to log an error
 */
static inline void logErrorMessage(myFMU2* fmu, fmi2String message) {
    if (logCategoryActive(fmu, logStatusError)) {
        fmu->logger(
            fmu->componentEnvironment,
            fmu->instanceName,
            fmi2Error,
            LOG_ERROR,
            message
        );
    }
}

/**
 * helper function to calculate absolute value
 */
static inline fmi2Real absReal(fmi2Real value) {
    return value >= 0 ? value : -value;
}

/**
 * helper function to calculated difference
 */
static inline fmi2Boolean areEqualReals(fmi2Real a, fmi2Real b) {
    return absReal(a-b) < EPSILON;
}

/**
 * Identify fmi2TypesPlatform.h
 */
const char* fmi2GetTypesPlatform(void) {
    return fmi2TypesPlatform;
}

/**
 * Identify fmi2Functions.h
 */
const char* fmi2GetVersion(void) {
    return fmi2Version;
}

/**
 * Control which categories will be logged
 * The following categories are defined (see fmi2 2.2.4)
 * - logEvents    
 * - logSingularLinearSystems    // not supported
 * - logNonlinearSystems         // not supported
 * - logDynamicStateSelection    // not supported 
 * - logStatusWarning            
 * - logStatusDiscard            // not supported 
 * - logStatusError              
 * - logStatusFatal              // not supported 
 * - logStatusPending            // not supported 
 * - logAll 
 */
fmi2Status fmi2SetDebugLogging(fmi2Component c,
                               fmi2Boolean loggingOn,
                               size_t nCategories,
                               const fmi2String categories[]) {
    /* validate fmi2Component */
    if (!c) return fmi2Error;

    /* validate categories */
    if (nCategories > 0 && categories == NULL) return fmi2Error;

    myFMU2* myfmu2 = (myFMU2*)c;

    /* set log categories */
    myfmu2->logLevel = logNone; // disable all categories

    if (loggingOn) {
        if (nCategories==0) {
            /* set all categrories */
            myfmu2->logLevel = logAll;
        } else {
            /* set all specified categories */
            for(size_t i=0; i<nCategories; i++) {
                if (strcmp(categories[i],        "logEvents")                == 0) {
                    myfmu2->logLevel |= logEvents;
                } else if (strcmp(categories[i], "logSingularLinearSystems") == 0) {
                    myfmu2->logLevel |= logSingularLinearSystems;
                } else if (strcmp(categories[i], "logNonlinearSystems")      == 0) {
                    myfmu2->logLevel |= logNonlinearSystems;
                } else if (strcmp(categories[i], "logDynamicStateSelection") == 0) {
                    myfmu2->logLevel |= logDynamicStateSelection;
                } else if (strcmp(categories[i], "logStatusWarning")         == 0) {
                    myfmu2->logLevel |= logStatusWarning;
                } else if (strcmp(categories[i], "logStatusDiscard")         == 0) {
                    myfmu2->logLevel |= logStatusDiscard;
                } else if (strcmp(categories[i], "logStatusError")           == 0) {
                    myfmu2->logLevel |= logStatusError;
                } else if (strcmp(categories[i], "logStatusFatal")           == 0) {
                    myfmu2->logLevel |= logStatusFatal;
                } else if (strcmp(categories[i], "logStatusPending")         == 0) {
                    myfmu2->logLevel |= logStatusPending;
                } else if (strcmp(categories[i], "logAll")                   == 0) {
                    myfmu2->logLevel |= logAll;
                }
                /* silently ignore unknown log categories */
            }
        }
    }
    return fmi2OK;
}

/**
 * Create a new instance of an FMU2
 */
fmi2Component fmi2Instantiate(fmi2String instanceName,
                              fmi2Type fmuType,
                              fmi2String fmuGUID,
                              fmi2String fmuResourceLocation,
                              const fmi2CallbackFunctions* functions,
                              fmi2Boolean visible,
                              fmi2Boolean loggingOn) {
    /* are functions implemented ? */
    if (!functions) return NULL;

    /* is logger implemented ? */
    if (!functions->logger) return NULL;

    /* is allocateMemory implemented ? */
    if (!functions->allocateMemory) {
        fmi2String message = "allocateMemory not implemented.";
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, LOG_ERROR, message);
        return NULL;
    }
    /* is freeMemory implemented ? */
    if (!functions->freeMemory) {
        fmi2String message = "freeMemory not implemented.";
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, LOG_ERROR, message);
        return NULL;
    }
    /* compare model guid against modelDescription guid */
    if (strcmp(fmuGUID, GUID) != 0) {
        fmi2String message = "GUIDs don't match.";
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, LOG_ERROR, message);
        return NULL;
    }
    /* create an FMU2 instance */
    myFMU2* myfmu2 = functions->allocateMemory(1,sizeof(myFMU2));
    if (!myfmu2) {
        fmi2String message = "Unable to allocate memory for FMU2 instance.";
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, LOG_ERROR, message);
        return NULL;
    }
    memset(myfmu2, 0, sizeof(myFMU2)); ///> initialize all fields to zero to avoid garbage values
    /* set fmu2 callbacks */
    myfmu2->logger               = functions->logger;
    myfmu2->allocateMemory       = functions->allocateMemory;
    myfmu2->freeMemory           = functions->freeMemory;
    myfmu2->stepFinished         = NULL;                            ///> only synchronous co-simulation support
    myfmu2->componentEnvironment = functions->componentEnvironment; ///> No check needed, can be NULL
    /* create a model instance */
    myfmu2->model = newModel(NUMBER_OF_STATES, NUMBER_OF_INPUTS, NUMBER_OF_OUTPUTS, NUMBER_OF_PARAMS);
    if (!myfmu2->model) {
        fmi2String message = "Unable to allocate memory for model instance.";
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, LOG_ERROR, message);
        fmi2FreeInstance((fmi2Component)myfmu2);
        return NULL;
    }
    /* fill the fmu with data */
    myfmu2->instanceName   = strdup(instanceName);
    if (!myfmu2->instanceName) {
        fmi2String message = "Unable to allocate memory for instance name.";
        functions->logger(functions->componentEnvironment, instanceName, fmi2Error, LOG_ERROR, message);
        fmi2FreeInstance((fmi2Component)myfmu2);
        return NULL;
    }
    myfmu2->fmuType   = fmuType;
    myfmu2->logLevel  = logStatusError; // set logStatusError per default
    myfmu2->time      = 0.0;

    /* fmu succsessfully instantiated */
    myfmu2->state = stateInstantiated;
    return (fmi2Component)myfmu2;
}

/**
 * Free an FMU2 instance
 */
void fmi2FreeInstance(fmi2Component c) {
    if (c) {
        myFMU2* myfmu2 = (myFMU2*)c;
        if (myfmu2->instanceName) myfmu2->freeMemory((void*)myfmu2->instanceName);
        freeModel(myfmu2->model);
        myfmu2->freeMemory((void*)myfmu2);
    }
}

/**
 * Set up an experiment
 */
fmi2Status fmi2SetupExperiment(fmi2Component c,
                               fmi2Boolean toleranceDefined,
                               fmi2Real tolerance,
                               fmi2Real startTime,
                               fmi2Boolean stopTimeDefined,
                               fmi2Real stopTime) {
    /* validate component */
    if (!c) return fmi2Error;

    /* unused parameter */
    (void)toleranceDefined;
    (void)tolerance;        // integration error tollerace. Irrelevant in co-simulation

    myFMU2* myfmu2 = (myFMU2*)c;

    /* fmu needs to be fully instantiated (fmi2 2.1.6) */
    if (!(myfmu2->state & stateInstantiated)) {
        fmi2String message = "FMU needs to be instantiated before calling fmi2SetupExperiment.";
        logErrorMessage(myfmu2, message);
        return fmi2Error;
    }

    /* fmu must not be in initialization (fmi2 2.1.6) */
    if (myfmu2->state & stateInitializationMode) {
        fmi2String message = "fmi2SetupExperiment must be called before fmu is initialized.";
        logErrorMessage(myfmu2, message);
        return fmi2Error;
    }
    
    if (stopTimeDefined) {
        if (stopTime > startTime) {
            myfmu2->stopTime  = stopTime;
        } else {
            fmi2String message = "stopTime must be greater than startTime.";
            logErrorMessage(myfmu2, message);
            return fmi2Error;
        }
    }
    myfmu2->startTime = startTime;
    myfmu2->time      = startTime;
    return fmi2OK;
}

/**
 * Enter initialization mode
 */
fmi2Status fmi2EnterInitializationMode(fmi2Component c) {

    /* validate component */
    if (!c) return fmi2Error;
    
    myFMU2* myfmu2 = (myFMU2*)c;

    /* fmu needs to be fully instantiated (fmi2 2.1.6) */
    if (!(myfmu2->state & stateInstantiated)) {
        fmi2String message = "FMU needs to be instantiated before calling fmi2EnterInitializationMode.";
        logErrorMessage(myfmu2, message);
        return fmi2Error;
    }

    /* set initialization mode */
    myfmu2->state |= stateInitializationMode;

    /* initialize states */
    memcpy(myfmu2->model->states, INITIAL_STATE, NUMBER_OF_STATES*sizeof(double));

    /* initialize parameters */
    memcpy(myfmu2->model->parameter, INITIAL_PARAMS, NUMBER_OF_PARAMS*sizeof(double));

    /* state equation */
    myfmu2->model->getStateDerivatives = MODEL_STATE_EQUATION;

    /* integrator */
    myfmu2->model->integrate = INTEGRATOR_METHOD;

    /* output equation */
    myfmu2->model->getOutputs = MODEL_OUTPUT_EQUATION;

    return fmi2OK;
}

/**
 * Exit initialization mode
 */
fmi2Status fmi2ExitInitializationMode(fmi2Component c) {
    /* validate component */
    if (!c) return fmi2Error;

    myFMU2* myfmu2 = (myFMU2*)c;

    if (!(myfmu2->state & stateInitializationMode)) {
        fmi2String message = "Need to call fmi2EnterInitializationMode before calling fmi2ExitInitializationMode.";
        logErrorMessage(myfmu2, message);
        return fmi2Error;
    }

    /* set initilized */
    myfmu2->state &= ~stateInitializationMode;
    myfmu2->state |= stateInitialized;
    return fmi2OK;
}

/**
 * terminate the fmu
 */
fmi2Status fmi2Terminate(fmi2Component c) {
    // TODO
    return fmi2OK;
}

/**
 * reset the fmu
 */
fmi2Status fmi2Reset(fmi2Component c) {
    // TODO
    return fmi2OK;
}

/** 
 * set a real value
 */
fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {
    /* vallidate component */
    if (!c) return fmi2Error;

    /* validate value refrence*/
    if (nvr > 0 && (!vr || !value)) return fmi2Error;

    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate fmu state */
    if (!(myfmu2->state & (stateInstantiated | stateInitializationMode ))) {
        fmi2String message = "FMU needs to be in initialization mode to set and get values.";
        logErrorMessage(myfmu2, message);
        return fmi2Error;
    }

    /* find value reference in model */
    /* valueReference is sorted parameter, inputs, outputs */
    for (size_t k=0; k<nvr; k++) {
        fmi2ValueReference VR = vr[k];
        if (VR < NUMBER_OF_PARAMS) {
            /* vr is a parameter */
            if (myfmu2->state & stateInitializationMode) {
                myfmu2->model->parameter[VR] = value[k];
                // TODO: call modelValidateParameter()
            } else {
                fmi2String message = "Parameter can only be set during initialization.";
                logErrorMessage(myfmu2, message);
                return fmi2Error;
            }
        } else if (VR < (NUMBER_OF_PARAMS + NUMBER_OF_INPUTS)) {
            /* vr is an input */
            myfmu2->model->inputs[VR-NUMBER_OF_PARAMS] = value[k];
            // TODO: call modelValidateInput
        } else {
            fmi2String message = "Value reference is out of scope. Only Parameter and Inputs can be set.";
            logErrorMessage(myfmu2, message);
            return fmi2Error;
        }
    }
    return fmi2OK;
}

/**
 * set an integer value
 */
fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
    /* no integer values to set */
    return fmi2OK;
}

 /**
 * set a boolean value
 */
fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
    /* no boolean values to set */
    return fmi2OK;
}

 /**
 * set a string value
 */
fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String  value[]) {
    /* no string values to set */
    return fmi2OK;
}

/**
 * get a real value
 */
fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
    /* vallidate component */
    if (!c) return fmi2Error;

    /* validate value refrence*/
    if (nvr > 0 && (!vr || !value)) return fmi2Error;

    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate fmu state */
    if (!(myfmu2->state & (stateInstantiated | stateInitializationMode))) {
        fmi2String message = "FMU needs to be in initialization mode to set and get values.";
        logErrorMessage(myfmu2, message);
        return fmi2Error;
    }

    /* find value reference in model */
    /* valueReference is sorted parameter, inputs, outputs */
    for (size_t k=0; k<nvr; k++) {
        fmi2ValueReference VR = vr[k];
        if (VR < NUMBER_OF_PARAMS) {
            /* vr is a parameter */
            value[k] = myfmu2->model->parameter[VR];
        } else if (VR < (NUMBER_OF_PARAMS + NUMBER_OF_INPUTS)) {
            /* vr is an input */
            value[k] = myfmu2->model->inputs[VR-NUMBER_OF_PARAMS];
        } else if (VR < (NUMBER_OF_PARAMS + NUMBER_OF_INPUTS + NUMBER_OF_OUTPUTS)) {
            /* vr is an output*/
            value[k] = myfmu2->model->outputs[VR-(NUMBER_OF_PARAMS+NUMBER_OF_INPUTS)];
        } else {
            fmi2String message = "Value reference is out of scope.";
            logErrorMessage(myfmu2, message);
            return fmi2Error;
        }
    }
    return fmi2OK;
}

/**
 * get an integer value
 */
fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
    /* no integer values to get */
    return fmi2OK;
}

/** get a boolean value  */
fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
    /* no boolean values to get */
    return fmi2OK;
}

/**
 * get a string value
 */
fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String  value[]) {
    /* no string values to get */
    return fmi2OK;
}

/**
 * Do a simulation time step x(t) -> x(t+dt)
 * 
 * Note: FMU expects a fixed communicationStepSize
 */
fmi2Status fmi2DoStep(fmi2Component c, fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize, fmi2Boolean noSetFMUStatePriorToCurrentPoint) {

    /* unused parameter */
    (void)noSetFMUStatePriorToCurrentPoint;

    /* validate component */
    // TODO: log error
    if (!c) return fmi2Error;

    /* validate communicationStepSize */
    // TODO: log error
    if (communicationStepSize <= 0.0) return fmi2Error;

    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate state */
    // TODO: log error
    if (!(myfmu2->state & stateInitialized)) return fmi2Error;

    /* validate time */
    if (!(areEqualReals(myfmu2->time, currentCommunicationPoint))) {
        // TODO: log error
        return fmi2Error;
    }

    /* handle variable step size */
    fmi2Integer numberOfSteps = (fmi2Integer)(communicationStepSize/MAX_SOLVER_STEPSIZE);
    fmi2Real stepRemainder    = communicationStepSize - numberOfSteps*MAX_SOLVER_STEPSIZE;
    fmi2Real actualStepSize   = 0.0; // actual step initialized to 0 to prevent garbage

    /* do steps */
    actualStepSize = MAX_SOLVER_STEPSIZE;
    for (fmi2Integer k=0; k<numberOfSteps; k++) {
        myfmu2->model->getStateDerivatives(myfmu2->model);
        myfmu2->model->integrate(myfmu2->model, actualStepSize);
    }
    
    /* handle remainder step */
    if (stepRemainder >= EPSILON) {
        // only step if remainder is numerically significant
        actualStepSize = stepRemainder;
        myfmu2->model->getStateDerivatives(myfmu2->model);
        myfmu2->model->integrate(myfmu2->model, actualStepSize);
    }
    
    /* update internal clock */
    myfmu2->time += communicationStepSize;

    /* write outpus */
    myfmu2->model->getOutputs(myfmu2->model);

    return fmi2OK;
}