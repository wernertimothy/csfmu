
#include <string.h>
#include "fmi2Functions.h"
#include "config.h"
#include "model.h"

/**
 * An instance of the FMU2
 */
typedef struct {
    // TODO : sort by size to minimize padding
    /* meta data */
    fmi2String instanceName; ///> Name of the FMU
    fmi2Type fmuType;        ///> CS of ME

    /* experiment */
    fmi2Real startTime; ///> start time of experiment
    fmi2Real stopTime;  ///> stop time of experiment
    fmi2Real time;      ///> internal clock

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
 * TODO
 */
fmi2Status fmi2SetDebugLogging(fmi2Component c,
                               fmi2Boolean loggingOn,
                               size_t nCategories,
                               const fmi2String categories[]) {
    // TODO
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
    if (!functions) {
        return NULL;
    }
    /* is logger implemented ? */
    if (!functions->logger) {
        return NULL;
    }
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
    myfmu2->model = newModel(NUMBER_OF_STATES, NUMBER_OF_INPUTS);
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
    myfmu2->fmuType = fmuType;
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
    if (!c) {
        return fmi2Error;
    }
    /* unused parameter */
    (void)toleranceDefined;
    (void)tolerance;        // integration error tollerace. Irrelevant in co-simulation

    myFMU2* myfmu2 = (myFMU2*)c;
    
    if (stopTimeDefined) {
        if (stopTime > startTime) {
            myfmu2->stopTime  = stopTime;
        } else {
            fmi2String message = "stopTime must be greater than startTime.";
            myfmu2->logger(
                myfmu2->componentEnvironment,
                myfmu2->instanceName,
                fmi2Error,
                LOG_ERROR,
                message
            );
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
    // TODO
    return fmi2OK;
}

/**
 * Exit initialization mode
 */
fmi2Status fmi2ExitInitializationMode(fmi2Component c) {
    // TODO
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