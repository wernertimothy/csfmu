
#include <string.h>
#include "fmi2Functions.h"
#include "model_config.h"
#include "model.h"

/*
TODOs:
- better way to check allowed state (bitmask)
- implement all fmu states
- should stubbed functions return warning or error ?
*/

#define EPSILON 1e-9 // numerical significance boarder

/***************************************************
 * Type Definitions and Enums
 ***************************************************/

/**
 * An instance of the FMU2
 */
typedef struct {
    // TODO : sort by size to minimize padding
    /* meta data */
    fmi2String instanceName; ///> Name of the FMU
    fmi2Type fmuType;        ///> CS or ME

    /* experiment */
    fmi2Real startTime;      ///> start time of experiment
    fmi2Real stopTime;       ///> stop time of experiment
    fmi2Real time;           ///> internal clock

    /* logging */
    unsigned int logLevel;   ///> bitmask of current log level

    /* fmu state */
    unsigned int state;      ///> tracks the current state of the fmu

    /* functions */
    fmi2CallbackLogger         logger;               ///> Logger callback
    fmi2CallbackAllocateMemory allocateMemory;       ///> allocate memory callback
    fmi2CallbackFreeMemory     freeMemory;           ///> free memory callback
    fmi2StepFinished           stepFinished;         ///> asynchronous co-simualtion
    fmi2ComponentEnvironment   componentEnvironment; ///> integration tool context

    /* model */
    modelInstance* model;    ///> Instance of the model
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
 * TODO: make complete
 */
typedef enum {
    stateInstantiated       = 1 << 0, // set by fmi2Instantiate
    stateInitializationMode = 1 << 1, // set by fmi2EnterInitializationMode
    stateInitialized        = 1 << 2, // set by fmi2ExitInitializationMode
} stateCategory;

/***************************************************
 * Helper Functions
 ***************************************************/

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
 * helper function to log a warning
 */
static inline void logWarningMessage(myFMU2* fmu, fmi2String message) {
    if (logCategoryActive(fmu, logStatusWarning)) {
        fmu->logger(
            fmu->componentEnvironment,
            fmu->instanceName,
            fmi2Warning,
            LOG_WARNING,
            message
        );
    }
}

/**
 * helper function to log an event
 */
static inline void logEventMessage(myFMU2* fmu, fmi2String message) {
    if (logCategoryActive(fmu, logEvents)) {
        fmu->logger(
            fmu->componentEnvironment,
            fmu->instanceName,
            fmi2OK,
            LOG_EVENT,
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

/***************************************************
 * FMI 2.0 Inquiry Functions
 ***************************************************/

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

/***************************************************
 * FMI 2.0 Creation and Destruction
 ***************************************************/

/**
 * Creates an instance of the fmu.
 *
 * Validates required callbacks and GUID, allocates memory using the integration tool's
 * allocator, and initializes the internal model.
 */
fmi2Component fmi2Instantiate(fmi2String instanceName,
                              fmi2Type fmuType,
                              fmi2String fmuGUID,
                              fmi2String fmuResourceLocation,
                              const fmi2CallbackFunctions* functions,
                              fmi2Boolean visible,
                              fmi2Boolean loggingOn) {
    /* unused parameters */
    (void)fmuResourceLocation;
    (void)visible;

    /* validate logger callback before allocation */
    if (!functions || !functions->logger) {
        return NULL;
    }

    /* validate required callbacks */
    if (!functions->allocateMemory || !functions->freeMemory) {
        functions->logger(functions->componentEnvironment, instanceName,
                         fmi2Error, LOG_ERROR,
                         "Missing required memory callbacks");
        return NULL;
    }

    /* validate GUID */
    if (strcmp(fmuGUID, GUID) != 0) {
        functions->logger(functions->componentEnvironment, instanceName,
                         fmi2Error, LOG_ERROR,
                         "GUID mismatch");
        return NULL;
    }

    /* allocate memory */
    myFMU2* myfmu2 = (myFMU2*)functions->allocateMemory(1, sizeof(myFMU2));
    if (!myfmu2) {
        functions->logger(functions->componentEnvironment, instanceName,
                         fmi2Error, LOG_ERROR,
                         "Memory allocation failed");
        return NULL;
    }

    /* zero-initialize to prevent uninitialized pointer bugs */
    memset(myfmu2, 0, sizeof(myFMU2));

    /* store callbacks immediately (needed for error recovery) */
    myfmu2->logger               = functions->logger;
    myfmu2->allocateMemory       = functions->allocateMemory;
    myfmu2->freeMemory           = functions->freeMemory;
    myfmu2->stepFinished         = functions->stepFinished;
    myfmu2->componentEnvironment = functions->componentEnvironment;

    /* duplicate instance name */
    myfmu2->instanceName = strdup(instanceName);
    if (!myfmu2->instanceName) {
        fmi2FreeInstance((fmi2Component)myfmu2);
        return NULL;
    }

    /* store FMU type */
    myfmu2->fmuType = fmuType;

    /* initialize logging */
    if (loggingOn) {
        myfmu2->logLevel = logAll;
    } else {
        myfmu2->logLevel = logNone;
    }

    /* create model instance */
    myfmu2->model = newModel(NUMBER_OF_STATES, NUMBER_OF_INPUTS, NUMBER_OF_OUTPUTS, NUMBER_OF_PARAMS);
    if (!myfmu2->model) {
        fmi2FreeInstance((fmi2Component)myfmu2);
        return NULL;
    }

    /* set state */
    myfmu2->state = stateInstantiated;

    logEventMessage(myfmu2, "FMU instantiated succsessfully.");
    return (fmi2Component)myfmu2;
}

/**
 * Frees the FMU instance and all associated memory.
 */
void fmi2FreeInstance(fmi2Component c) {
    if (c) {
        myFMU2* myfmu2 = (myFMU2*)c;

        logEventMessage(myfmu2, "FMU will be freed from memory.");

        /* free instance name */
        if (myfmu2->instanceName) {
            free((void*)myfmu2->instanceName);
        }

        /* free model */
        freeModel(myfmu2->model);
        myfmu2->freeMemory((void*)myfmu2);
    }
}

/***************************************************
 * FMI 2.0 Setup and Initialization
 ***************************************************/

/**
 * Set up an experiment
 */
fmi2Status fmi2SetupExperiment(fmi2Component c,
                               fmi2Boolean toleranceDefined,
                               fmi2Real tolerance,
                               fmi2Real startTime,
                               fmi2Boolean stopTimeDefined,
                               fmi2Real stopTime) {
                                   
    /* unused parameter */
    (void)toleranceDefined;
    (void)tolerance;         // integration error tollerace. Irrelevant in co-simulation
                                   
    /* validate component */
    if (!c) {
        // can't log the error
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate state */
    if (!(myfmu2->state & stateInstantiated)) {
        logErrorMessage(myfmu2, "FMU needs to be fully instantiated.");
        return fmi2Error;
    }

    /* validate time window */
    if (stopTimeDefined && stopTime < startTime) {
        logErrorMessage(myfmu2, "stopTime must be >= startTime");
        return fmi2Error;
    }

    /* store experiment parameters */
    myfmu2->startTime = startTime;
    myfmu2->stopTime  = stopTime;
    myfmu2->time      = startTime;

    logEventMessage(myfmu2, "FMU Experiment set up succsessfully.");
    return fmi2OK;
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
    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate state */
    // can be called in every state

    /* validate categories array */
    if (nCategories > 0 && categories == NULL) {
        logErrorMessage(myfmu2, "Invalid log categories.");
        return fmi2Error;
    }

    /* set log level based on loggingOn flag */
    if (!loggingOn) {
        myfmu2->logLevel = logNone;
        logEventMessage(myfmu2, "log level set succsessfully.");
        return fmi2OK;
    }

    /* enable all categories if nCategories == 0 */
    if (nCategories == 0) {
        myfmu2->logLevel = logAll;
        logEventMessage(myfmu2, "Log level set succsessfully.");
        return fmi2OK;
    }

    /* enable specified categories */
    myfmu2->logLevel = logNone;
    for (size_t i = 0; i < nCategories; i++) {
        const char* cat = categories[i];

        if (strcmp(cat, "logAll") == 0) {
            logEventMessage(myfmu2, "Log level set succsessfully.");
            myfmu2->logLevel = logAll;
            return fmi2OK; // No need to process remaining categories
        } else if (strcmp(cat, "logEvents") == 0) {
            myfmu2->logLevel |= logEvents;
        } else if (strcmp(cat, "logSingularLinearSystems") == 0) {
            myfmu2->logLevel |= logSingularLinearSystems;
        } else if (strcmp(cat, "logNonlinearSystems") == 0) {
            myfmu2->logLevel |= logNonlinearSystems;
        } else if (strcmp(cat, "logDynamicStateSelection") == 0) {
            myfmu2->logLevel |= logDynamicStateSelection;
        } else if (strcmp(cat, "logStatusWarning") == 0) {
            myfmu2->logLevel |= logStatusWarning;
        } else if (strcmp(cat, "logStatusDiscard") == 0) {
            myfmu2->logLevel |= logStatusDiscard;
        } else if (strcmp(cat, "logStatusError") == 0) {
            myfmu2->logLevel |= logStatusError;
        } else if (strcmp(cat, "logStatusFatal") == 0) {
            myfmu2->logLevel |= logStatusFatal;
        } else if (strcmp(cat, "logStatusPending") == 0) {
            myfmu2->logLevel |= logStatusPending;
        }
        // Unknown categories are silently ignored
    }

    logEventMessage(myfmu2, "Log level set succsessfully.");
    return fmi2OK;
}

/**
 * Enter initialization mode
 *
 * Sets up default initial values for states, parameters, and function pointers.
 * Integration tool can override these defaults by calling fmi2SetReal/etc before
 * calling fmi2ExitInitializationMode.
 */
fmi2Status fmi2EnterInitializationMode(fmi2Component c) {
    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate state */
    if (!(myfmu2->state & stateInstantiated)) {
        logErrorMessage(myfmu2, "fmi2EnterInitializationMode can only be called after fmi2Instantiate and fmi2SetupExperiment");
        return fmi2Error;
    }

    /* set default parameter values */
    memcpy(myfmu2->model->parameter, INITIAL_PARAMS, NUMBER_OF_PARAMS * sizeof(double));

    /* set default initial states */
    memcpy(myfmu2->model->states, INITIAL_STATE, NUMBER_OF_STATES * sizeof(double));

    /* set function pointers */
    myfmu2->model->getStateDerivatives = MODEL_STATE_EQUATION;
    myfmu2->model->integrate           = INTEGRATOR_METHOD;
    myfmu2->model->getOutputs          = MODEL_OUTPUT_EQUATION;

    /* update state */
    myfmu2->state |= stateInitializationMode;

    logEventMessage(myfmu2, "FMU entered initialization mode.");
    return fmi2OK;
}

/**
 * Exit initialization mode
 *
 * Finalizes initialization and transitions to step mode.
 */
fmi2Status fmi2ExitInitializationMode(fmi2Component c) {
    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate state */
    if (!(myfmu2->state & stateInitializationMode)) {
        logErrorMessage(myfmu2, "fmi2ExitInitializationMode can only be called after fmi2EnterInitializationMode");
        return fmi2Error;
    }

    /* transition state */
    myfmu2->state &= ~stateInitializationMode;
    myfmu2->state |= stateInitialized;

    logEventMessage(myfmu2, "FMU initilized succsessfully");
    return fmi2OK;
}

/***************************************************
 * FMI 2.0 Getting and Setting Variables
 ***************************************************/

/**
 * set a real value
 */
fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {
    /* vallidate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate state */
    if (!(myfmu2->state & (stateInstantiated | stateInitializationMode ))) {
        logErrorMessage(myfmu2, "FMU needs to be fully instantiated and in initilaization mode to set values.");
        return fmi2Error;
    }

    /* validate value refrence*/
    if (nvr > 0 && (!vr || !value)) {
        logErrorMessage(myfmu2, "Invalid value reference or values.");
        return fmi2Error;
    }

    /* set values */
    for (size_t k=0; k<nvr; k++) {
        fmi2ValueReference VR = vr[k];

        /* parameters can only be set during initialization mode */
        if (VR < NUMBER_OF_PARAMS) {
            myfmu2->model->parameter[VR] = value[k];
        }
        /* inputs can be set anytime after instantiation */
        else if (VR < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS) {
            size_t inputIndex = VR - NUMBER_OF_PARAMS;
            myfmu2->model->inputs[inputIndex] = value[k];
        }
        else {
            logErrorMessage(myfmu2, "The value reference appears to be out of scope for setting a value.");
            return fmi2Error;
        }
    }
    logEventMessage(myfmu2, "Values succsessfully set.");
    return fmi2OK;
}

/**
 * set integer values
 */
fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/**
 * set boolean values
 */
fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/**
 * set string values
 */
fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String  value[]) {
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/**
 * get a real value
 */
fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate state */
    if (!(myfmu2->state & (stateInstantiated | stateInitializationMode))) {
        logErrorMessage(myfmu2, "FMU needs to be fully instantiated and in initiliaztion mode to get values.");
        return fmi2Error;
    }

    /* validate value reference */
    if (nvr > 0 && (!vr || !value)) {
        logErrorMessage(myfmu2, "Invalid value reference or value.");
        return fmi2Error;
    }

    /* get values */
    for (size_t k=0; k<nvr; k++) {
        fmi2ValueReference VR = vr[k];

        /* parameters */
        if (VR < NUMBER_OF_PARAMS) {
            value[k] = myfmu2->model->parameter[VR];
        }
        /* inputs */
        else if (VR < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS) {
            size_t inputIndex = VR - NUMBER_OF_PARAMS;
            value[k] = myfmu2->model->inputs[inputIndex];
        }
        /* outputs */
        else if (VR < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS + NUMBER_OF_OUTPUTS) {
            size_t outputIndex = VR - NUMBER_OF_PARAMS - NUMBER_OF_INPUTS;
            value[k] = myfmu2->model->outputs[outputIndex];
        }
        else {
            logErrorMessage(myfmu2, "The value reference appears to be out of scope for getting values.");
            return fmi2Error;
        }
    }
    logEventMessage(myfmu2, "Values succsessfully get.");
    return fmi2OK;
}

/**
 * get integer values
 */
fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}


/**
 * get boolean values
 */
fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}


/**
 * get string values
 */
fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String  value[]) {
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/***************************************************
 * FMI 2.0 Co-Simulation Execution
 ***************************************************/

/**
 * Do a simulation time step x(t) -> x(t+dt)
 *
 * Note: FMU expects a fixed communicationStepSize
 */
fmi2Status fmi2DoStep(fmi2Component c, fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize, fmi2Boolean noSetFMUStatePriorToCurrentPoint) {

    /* unused parameter */
    (void)noSetFMUStatePriorToCurrentPoint;

    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    /* validate state */
    if (!(myfmu2->state & stateInitialized)) {
        logErrorMessage(myfmu2, "FMU needs to be fully initialized.");
        return fmi2Error;
    }

    /* validate communicationStepSize */
    if (communicationStepSize <= 0.0) {
        logErrorMessage(myfmu2, "communicationStepSize must be greater than zero.");
        return fmi2Error;
    }

    /* validate time */
    if (!(areEqualReals(myfmu2->time, currentCommunicationPoint))) {
        logErrorMessage(myfmu2, "FMU internal clock does not match environment clock.");
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

    logEventMessage(myfmu2, "FMU stepped succsessfully.");
    return fmi2OK;
}

/**
 * Cancel step (asynchronous co-simulation only - not supported)
 */
fmi2Status fmi2CancelStep(fmi2Component c) {
    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2CancelStep is not supported.");
    return fmi2Warning;
}

/***************************************************
 * FMI 2.0 Simulation Control (Stubs)
 ***************************************************/

/**
 * Terminate the FMU (not implemented)
 */
fmi2Status fmi2Terminate(fmi2Component c) {
    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2Terminated is not supported.");
    return fmi2Warning;
}

/**
 * Reset the FMU (not implemented)
 */
fmi2Status fmi2Reset(fmi2Component c) {
    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2Reset is not supported.");
    return fmi2Warning;
}

/***************************************************
 * FMI 2.0 Status Queries (Stubs)
 ***************************************************/

fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind s, fmi2Status* value) {
    /* unused parameter */
    (void)s;
    (void)value;

    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2GetStatus is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetRealStatus(fmi2Component c, const fmi2StatusKind s, fmi2Real* value) {
    /* unused parameter */
    (void)s;
    (void)value;

    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2GetRealStatus is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer* value) {
    /* unused parameter */
    (void)s;
    (void)value;

    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2GetIntegerStatus is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value) {
    /* unused parameter */
    (void)s;
    (void)value;

    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2GetBooleanStatus is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetStringStatus(fmi2Component c, const fmi2StatusKind s, fmi2String* value) {
    /* unused parameter */
    (void)s;
    (void)value;

    /* validate component */
    if (!c) {
        // can't log error
        return fmi2Error;
    }
    
    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2GetStringStatus is not supported.");
    return fmi2Warning;
}

/***************************************************
 * FMI 2.0 FMU State Management (Stubs)
 ***************************************************/

fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) {
    (void)c;
    (void)FMUstate;
    // TODO: log warning
    return fmi2Error;
}

fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate FMUstate) {
    (void)c;
    (void)FMUstate;
    // TODO: log warning
    return fmi2Error;
}

fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate) {
    (void)c;
    (void)FMUstate;
    // TODO: log warning
    return fmi2Error;
}

fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate FMUstate, size_t* size) {
    (void)c;
    (void)FMUstate;
    (void)size;
    // TODO: log warning
    return fmi2Error;
}

fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size) {
    (void)c;
    (void)FMUstate;
    (void)serializedState;
    (void)size;
    // TODO: log warning
    return fmi2Error;
}

fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate) {
    (void)c;
    (void)serializedState;
    (void)size;
    (void)FMUstate;
    // TODO: log warning
    return fmi2Error;
}

/***************************************************
 * FMI 2.0 Directional Derivatives (Stubs)
 ***************************************************/

fmi2Status fmi2GetDirectionalDerivative(fmi2Component c,
                                        const fmi2ValueReference vUnknown_ref[], size_t nUnknown,
                                        const fmi2ValueReference vKnown_ref[], size_t nKnown,
                                        const fmi2Real dvKnown[], fmi2Real dvUnknown[]) {
    (void)c;
    (void)vUnknown_ref;
    (void)nUnknown;
    (void)vKnown_ref;
    (void)nKnown;
    (void)dvKnown;
    (void)dvUnknown;
    // TODO: log warning
    return fmi2Error;
}

fmi2Status fmi2SetRealInputDerivatives(fmi2Component c, const fmi2ValueReference vr[], size_t nvr,
                                       const fmi2Integer order[], const fmi2Real value[]) {
    (void)c;
    (void)vr;
    (void)nvr;
    (void)order;
    (void)value;
    // TODO: log warning
    return fmi2Error;
}

fmi2Status fmi2GetRealOutputDerivatives(fmi2Component c, const fmi2ValueReference vr[], size_t nvr,
                                        const fmi2Integer order[], fmi2Real value[]) {
    (void)c;
    (void)vr;
    (void)nvr;
    (void)order;
    (void)value;
    // TODO: log warning
    return fmi2Error;
}

/***************************************************
 * FMI 2.0 Model Exchange Functions (Stubs)
 * Not applicable to co-simulation FMUs
 ***************************************************/

fmi2Status fmi2SetTime(fmi2Component c, fmi2Real time) {
    (void)c;
    (void)time;
    return fmi2Error;
}

fmi2Status fmi2SetContinuousStates(fmi2Component c, const fmi2Real x[], size_t nx) {
    (void)c;
    (void)x;
    (void)nx;
    return fmi2Error;
}

fmi2Status fmi2GetDerivatives(fmi2Component c, fmi2Real derivatives[], size_t nx) {
    (void)c;
    (void)derivatives;
    (void)nx;
    return fmi2Error;
}

fmi2Status fmi2GetEventIndicators(fmi2Component c, fmi2Real eventIndicators[], size_t ni) {
    (void)c;
    (void)eventIndicators;
    (void)ni;
    return fmi2Error;
}

fmi2Status fmi2GetContinuousStates(fmi2Component c, fmi2Real x[], size_t nx) {
    (void)c;
    (void)x;
    (void)nx;
    return fmi2Error;
}

fmi2Status fmi2GetNominalsOfContinuousStates(fmi2Component c, fmi2Real x_nominal[], size_t nx) {
    (void)c;
    (void)x_nominal;
    (void)nx;
    return fmi2Error;
}

fmi2Status fmi2CompletedIntegratorStep(fmi2Component c, fmi2Boolean noSetFMUStatePriorToCurrentPoint,
                                       fmi2Boolean* enterEventMode, fmi2Boolean* terminateSimulation) {
    (void)c;
    (void)noSetFMUStatePriorToCurrentPoint;
    (void)enterEventMode;
    (void)terminateSimulation;
    return fmi2Error;
}

fmi2Status fmi2EnterEventMode(fmi2Component c) {
    (void)c;
    return fmi2Error;
}

fmi2Status fmi2NewDiscreteStates(fmi2Component c, fmi2EventInfo* eventInfo) {
    (void)c;
    (void)eventInfo;
    return fmi2Error;
}

fmi2Status fmi2EnterContinuousTimeMode(fmi2Component c) {
    (void)c;
    return fmi2Error;
}
