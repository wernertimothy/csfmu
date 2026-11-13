/**************************************************************************************************
*                                                                                                 *
* This file is part of fmuTemplate.                                                               *
*                                                                                                 *
* fmuTemplate -- A template for Co-Simulation FMUs written in C.                                  *
* Copyright (C) 2025 Timothy Werner                                                               *
*                                                                                                 *
* SPDX-License-Identifier: MIT                                                                    *
*                                                                                                 *
**************************************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "fmi2Functions.h"
#include "model.h"
#include "integrator.h"

/* Numerical tolerance for floating-point comparisons */
#define EPSILON 1e-9

/* Buffer size for formatted log messages */
#define LOG_BUFFER_SIZE 512

/***************************************************
 * FMI2 Function State Validation Bitmasks
 *
 * These macros define which FMU states are valid
 * for calling each FMI function. Used for runtime
 * state machine validation.
 ***************************************************/

#define STATE_fmi2SetupExperiment (stateInstantiated)
#define STATE_fmi2SetDebugLogging (stateInstantiated | stateInitializationMode | stateStepComplete | \
                                   stateStepInProgress | stateStepFailed | stateStepCanceled | \
                                   stateTerminated | stateError | stateFatal)
#define STATE_fmi2EnterInitializationMode (stateInstantiated)
#define STATE_fmi2ExitInitializationMode (stateInitializationMode)
#define STATE_fmi2SetReal (stateInstantiated | stateInitializationMode | stateStepComplete)
#define STATE_fmi2GetReal (stateInstantiated | stateStepComplete | stateStepFailed | \
                           stateStepCanceled | stateTerminated | stateError)
#define STATE_fmi2DoStep (stateStepComplete)
#define STATE_fmi2Terminate (stateStepComplete | stateStepFailed)

/***************************************************
 * Type Definitions and Enums
 ***************************************************/

/**
 * FMU instance structure (myFMU2)
 *
 * Wraps the model-agnostic modelInstance with FMI-specific
 * metadata, callbacks, and state tracking.
 */
typedef struct {
    /* Metadata */
    fmi2String instanceName;
    fmi2Type   fmuType;

    /* Experiment time bounds */
    fmi2Real startTime;
    fmi2Real stopTime;
    fmi2Real time;

    /* Logging configuration */
    unsigned int logLevel;

    /* FMU state machine tracking */
    unsigned int state;

    /* Integration tool callbacks */
    fmi2CallbackLogger         logger;
    fmi2CallbackAllocateMemory allocateMemory;
    fmi2CallbackFreeMemory     freeMemory;
    fmi2StepFinished           stepFinished;
    fmi2ComponentEnvironment   componentEnvironment;

    /* Model instance (FMU-agnostic) */
    modelInstance* model;
} myFMU2;

/**
 * Log category enumeration
 *
 * Bitmask-based log levels matching FMI 2.0 specification.
 * Multiple categories can be enabled simultaneously via bitwise OR.
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
 * FMU state machine enumeration
 *
 * States are mutually exclusive (not accumulated like error flags).
 * Follows FMI 2.0 state machine specification for co-simulation.
 */
typedef enum {
    stateStartEnd           = 0,           /* Pseudo-state (not tracked) */
    stateInstantiated       = 1 << 0,  /* Set by fmi2Instantiate */
    stateInitializationMode = 1 << 1,  /* Set by fmi2EnterInitializationMode */
    stateStepComplete       = 1 << 2,  /* Set by fmi2ExitInitializationMode and fmi2DoStep */
    stateStepInProgress     = 1 << 3,  /* Set by asynchronous fmi2DoStep */
    stateStepFailed         = 1 << 4,  /* If fmi2DoStep returns fmi2Discard */
    stateStepCanceled       = 1 << 5,  /* Set by fmi2CancelStep */
    stateTerminated         = 1 << 6,  /* Set by fmi2Terminate */
    stateError              = 1 << 7,  /* Accumulated flag (not exclusive) */
    stateFatal              = 1 << 8,  /* Accumulated flag (not exclusive) */
} stateCategory;

/***************************************************
 * Helper Functions
 ***************************************************/

/**
 * Check if current FMU state allows a function call
 */
static inline fmi2Boolean stateIsAllowed(unsigned int currentState, unsigned int expectedState)
{
    return (currentState & expectedState);
}

/**
 * Check if a log category is currently active
 */
static inline fmi2Boolean logCategoryIsActive(unsigned int logLevel, logCategory category)
{
    return (logLevel & category);
}

/**
 * Core logging function (variadic message support)
 *
 * Formats message with vsnprintf and forwards to integration tool's logger.
 */
static inline void logMessage(
    myFMU2* fmu,
    logCategory category,
    fmi2Status status,
    const char* categoryName,
    fmi2String message,
    va_list args)
{
    if (logCategoryIsActive(fmu->logLevel, category)) {
        char buffer[LOG_BUFFER_SIZE];
        vsnprintf(buffer, sizeof(buffer), message, args);
        /* Could detect truncation: if (sizeWritten >= sizeof(buffer)) */
        fmu->logger(
            fmu->componentEnvironment,
            fmu->instanceName,
            status,
            categoryName,
            buffer
        );
    }
}

/**
 * Log an error message
 */
static inline void logErrorMessage(myFMU2* fmu, fmi2String message, ...)
{
    va_list args;
    va_start(args, message);
    logMessage(fmu, logStatusError, fmi2Error, LOG_ERROR, message, args);
    va_end(args);
}

/**
 * Log a warning message
 */
static inline void logWarningMessage(myFMU2* fmu, fmi2String message, ...)
{
    va_list args;
    va_start(args, message);
    logMessage(fmu, logStatusWarning, fmi2Warning, LOG_WARNING, message, args);
    va_end(args);
}

/**
 * Log an event message
 */
static inline void logEventMessage(myFMU2* fmu, fmi2String message, ...)
{
    va_list args;
    va_start(args, message);
    logMessage(fmu, logEvents, fmi2OK, LOG_INFO, message, args);
    va_end(args);
}

/**
 * Compute absolute value of a real number
 */
static inline fmi2Real absReal(fmi2Real value)
{
    return value >= 0 ? value : -value;
}

/**
 * Check if two real numbers are equal within EPSILON tolerance
 */
static inline fmi2Boolean areEqualReals(fmi2Real a, fmi2Real b)
{
    return absReal(a - b) < EPSILON;
}

/***************************************************
 * FMI 2.0 Inquiry Functions
 ***************************************************/

/**
 * Returns the platform identifier from fmi2TypesPlatform.h
 */
const char* fmi2GetTypesPlatform(void)
{
    return fmi2TypesPlatform;
}

/**
 * Returns the FMI version from fmi2Functions.h
 */
const char* fmi2GetVersion(void)
{
    return fmi2Version;
}

/***************************************************
 * FMI 2.0 Creation and Destruction
 ***************************************************/

/**
 * Creates an instance of the FMU
 *
 * Implementation notes:
 * - Validates required callbacks (logger, allocateMemory, freeMemory) before allocation
 * - Validates GUID matches compile-time model GUID
 * - Uses integration tool's allocator for all memory
 * - Zero-initializes structure to prevent undefined behavior
 * - Creates model instance with compile-time dimensions
 * - On any failure, cleans up partial allocations via fmi2FreeInstance
 */
fmi2Component fmi2Instantiate(
    fmi2String instanceName,
    fmi2Type fmuType,
    fmi2String fmuGUID,
    fmi2String fmuResourceLocation,
    const fmi2CallbackFunctions* functions,
    fmi2Boolean visible,
    fmi2Boolean loggingOn)
{
    /* Unused parameters */
    (void)fmuResourceLocation;
    (void)visible;

    /* Validate logger callback before allocation */
    if (!functions || !functions->logger) {
        return NULL;
    }

    /* Validate required callbacks */
    if (!functions->allocateMemory || !functions->freeMemory) {
        functions->logger(functions->componentEnvironment, instanceName,
                         fmi2Error, LOG_ERROR,
                         "Missing required memory callbacks");
        return NULL;
    }

    /* Validate GUID */
    if (strcmp(fmuGUID, GUID) != 0) {
        functions->logger(functions->componentEnvironment, instanceName,
                         fmi2Error, LOG_ERROR,
                         "GUID mismatch");
        return NULL;
    }

    /* Allocate FMU instance */
    myFMU2* myfmu2 = (myFMU2*)functions->allocateMemory(1, sizeof(myFMU2));
    if (!myfmu2) {
        functions->logger(functions->componentEnvironment, instanceName,
                         fmi2Error, LOG_ERROR,
                         "Memory allocation failed");
        return NULL;
    }

    /* Zero-initialize to prevent uninitialized pointer bugs */
    memset(myfmu2, 0, sizeof(myFMU2));

    /* Store callbacks immediately (needed for error recovery) */
    myfmu2->logger = functions->logger;
    myfmu2->allocateMemory = functions->allocateMemory;
    myfmu2->freeMemory = functions->freeMemory;
    myfmu2->stepFinished = functions->stepFinished;
    myfmu2->componentEnvironment = functions->componentEnvironment;

    /* Duplicate instance name */
    myfmu2->instanceName = strdup(instanceName);
    if (!myfmu2->instanceName) {
        fmi2FreeInstance((fmi2Component)myfmu2);
        return NULL;
    }

    /* Store FMU type */
    myfmu2->fmuType = fmuType;

    /* Initialize logging */
    if (loggingOn) {
        myfmu2->logLevel = logAll;
    } else {
        myfmu2->logLevel = logNone;
    }

    /* Create model instance */
    myfmu2->model = newModel(NUMBER_OF_STATES, NUMBER_OF_INPUTS, NUMBER_OF_OUTPUTS, NUMBER_OF_PARAMS);
    if (!myfmu2->model) {
        fmi2FreeInstance((fmi2Component)myfmu2);
        return NULL;
    }

    /* Set initial state */
    myfmu2->state = stateInstantiated;

    logEventMessage(myfmu2, "FMU instantiated.");
    return (fmi2Component)myfmu2;
}

/**
 * Frees the FMU instance and all associated memory
 *
 * Implementation notes:
 * - NULL-safe (can be called with NULL component)
 * - Logs farewell message before cleanup
 * - Frees instance name (allocated by strdup)
 * - Frees model via freeModel()
 * - Uses integration tool's freeMemory callback
 */
void fmi2FreeInstance(fmi2Component c)
{
    if (!c) {
        return;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    logEventMessage(myfmu2, "FMU will be freed from memory.");

    /* Free instance name */
    free((void*)myfmu2->instanceName);

    /* Free model */
    freeModel(myfmu2->model);

    /* Free FMU instance using tool's allocator */
    myfmu2->freeMemory((void*)myfmu2);
}

/***************************************************
 * FMI 2.0 Setup and Initialization
 ***************************************************/

/**
 * Set up experiment time bounds
 *
 * Implementation notes:
 * - tolerance parameter ignored (not used in co-simulation)
 * - Validates stopTime >= startTime if defined
 * - Initializes internal clock to startTime
 */
fmi2Status fmi2SetupExperiment(
    fmi2Component c,
    fmi2Boolean toleranceDefined,
    fmi2Real tolerance,
    fmi2Real startTime,
    fmi2Boolean stopTimeDefined,
    fmi2Real stopTime)
{
    /* Unused parameters */
    (void)toleranceDefined;
    (void)tolerance;  /* Integration error tolerance - irrelevant in co-simulation */

    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Validate state */
    if (!stateIsAllowed(myfmu2->state, STATE_fmi2SetupExperiment)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Illegal call sequence for fmi2SetupExperiment.");
        return fmi2Error;
    }

    /* Validate time window */
    if (stopTimeDefined && stopTime < startTime) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "stopTime %.4f must be >= startTime", stopTime);
        return fmi2Error;
    }

    /* Store experiment parameters */
    myfmu2->startTime = startTime;
    myfmu2->stopTime = stopTime;
    myfmu2->time = startTime;

    logEventMessage(myfmu2, "FMU Experiment set up. Start = %.4f. End = %.4f", startTime, stopTime);
    return fmi2OK;
}

/**
 * Control which log categories are enabled
 *
 * FMI 2.0 standard categories (section 2.2.4):
 * - logEvents
 * - logSingularLinearSystems (not supported)
 * - logNonlinearSystems (not supported)
 * - logDynamicStateSelection (not supported)
 * - logStatusWarning
 * - logStatusDiscard (not supported)
 * - logStatusError
 * - logStatusFatal (not supported)
 * - logStatusPending (not supported)
 * - logAll
 *
 * Implementation notes:
 * - If loggingOn=false, disables all logging
 * - If nCategories=0, enables all categories
 * - Otherwise, enables only specified categories
 * - Unknown categories are silently ignored
 */
fmi2Status fmi2SetDebugLogging(
    fmi2Component c,
    fmi2Boolean loggingOn,
    size_t nCategories,
    const fmi2String categories[])
{
    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Validate state */
    if (!stateIsAllowed(myfmu2->state, STATE_fmi2SetDebugLogging)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Illegal call sequence for fmi2SetDebugLogging.");
        return fmi2Error;
    }

    /* Validate categories array */
    if (nCategories > 0 && categories == NULL) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Invalid log categories.");
        return fmi2Error;
    }

    /* Disable all logging if loggingOn=false */
    if (!loggingOn) {
        myfmu2->logLevel = logNone;
        logEventMessage(myfmu2, "log level set.");
        return fmi2OK;
    }

    /* Enable all categories if nCategories=0 */
    if (nCategories == 0) {
        myfmu2->logLevel = logAll;
        logEventMessage(myfmu2, "Log level set.");
        return fmi2OK;
    }

    /* Enable specified categories */
    myfmu2->logLevel = logNone;
    for (size_t i = 0; i < nCategories; i++) {
        const char* cat = categories[i];

        if (strcmp(cat, "logAll") == 0) {
            logEventMessage(myfmu2, "Log level set.");
            myfmu2->logLevel = logAll;
            return fmi2OK;  /* No need to process remaining categories */
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
        /* Unknown categories are silently ignored */
    }

    logEventMessage(myfmu2, "Log level set.");
    return fmi2OK;
}

/**
 * Enter initialization mode
 *
 * Sets default initial values for states, parameters, and function pointers
 * from compile-time configuration. Integration tool can override these by
 * calling fmi2SetReal/etc before fmi2ExitInitializationMode.
 *
 * Implementation notes:
 * - Copies INITIAL_PARAMS and INITIAL_STATE from model_config.h
 * - Binds function pointers from model_config.h macros
 * - State is now open for parameter/state overrides
 */
fmi2Status fmi2EnterInitializationMode(fmi2Component c)
{
    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Validate state */
    if (!stateIsAllowed(myfmu2->state, STATE_fmi2EnterInitializationMode)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Illegal call sequence for fmi2EnterInitializationMode.");
        return fmi2Error;
    }

    /* Set default parameter values */
    memcpy(myfmu2->model->parameter, INITIAL_PARAMS, NUMBER_OF_PARAMS * sizeof(double));

    /* Set default initial states */
    memcpy(myfmu2->model->states, INITIAL_STATE, NUMBER_OF_STATES * sizeof(double));

    /* Bind function pointers */
    myfmu2->model->getStateDerivatives = MODEL_STATE_EQUATION;
    myfmu2->model->integrate = INTEGRATOR_METHOD;
    myfmu2->model->getOutputs = MODEL_OUTPUT_EQUATION;

    /* Update state */
    myfmu2->state = stateInitializationMode;

    logEventMessage(myfmu2, "FMU entered initialization mode.");
    return fmi2OK;
}

/**
 * Exit initialization mode and finalize setup
 *
 * Transitions to step mode, ready for simulation.
 */
fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Validate state */
    if (!stateIsAllowed(myfmu2->state, STATE_fmi2ExitInitializationMode)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Illegal call sequence for fmi2ExitInitializationMode.");
        return fmi2Error;
    }

    /* Transition state */
    myfmu2->state = stateStepComplete;

    logEventMessage(myfmu2, "FMU initialized.");
    return fmi2OK;
}

/***************************************************
 * FMI 2.0 Getting and Setting Variables
 ***************************************************/

/**
 * Set real-valued variables (parameters or inputs)
 *
 * Implementation notes:
 * - Uses value reference arithmetic to determine variable type
 * - VR [0, NUMBER_OF_PARAMS) → parameters
 * - VR [NUMBER_OF_PARAMS, NUMBER_OF_PARAMS+NUMBER_OF_INPUTS) → inputs
 * - Parameters can only be set during initialization mode
 * - Inputs can be set anytime after instantiation
 */
fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[])
{
    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Validate state */
    if (!stateIsAllowed(myfmu2->state, STATE_fmi2SetReal)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Illegal call sequence for fmi2SetReal.");
        return fmi2Error;
    }

    /* Validate value reference */
    if (nvr > 0 && (!vr || !value)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Invalid value reference or value.");
        return fmi2Error;
    }

    /* Set values */
    for (size_t k = 0; k < nvr; k++) {
        fmi2ValueReference VR = vr[k];

        /* Parameters (can only be set during initialization) */
        if (VR < NUMBER_OF_PARAMS) {
            myfmu2->model->parameter[VR] = value[k];
        }
        /* Inputs (can be set anytime after instantiation) */
        else if (VR < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS) {
            size_t inputIndex = VR - NUMBER_OF_PARAMS;
            myfmu2->model->inputs[inputIndex] = value[k];
        } else {
            myfmu2->state |= stateError;
            logErrorMessage(myfmu2, "The value reference appears to be out of scope for setting a value.");
            return fmi2Error;
        }
    }

    logEventMessage(myfmu2, "Values set.");
    return fmi2OK;
}

/**
 * Set integer-valued variables (not implemented - stub)
 */
fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[])
{
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/**
 * Set boolean-valued variables (not implemented - stub)
 */
fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[])
{
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/**
 * Set string-valued variables (not implemented - stub)
 */
fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[])
{
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/**
 * Get real-valued variables (parameters, inputs, or outputs)
 *
 * Implementation notes:
 * - Uses value reference arithmetic to determine variable type
 * - VR [0, NUMBER_OF_PARAMS) → parameters
 * - VR [NUMBER_OF_PARAMS, NUMBER_OF_PARAMS+NUMBER_OF_INPUTS) → inputs
 * - VR [NUMBER_OF_PARAMS+NUMBER_OF_INPUTS, ...) → outputs
 * - This ordering must match modelDescription.xml
 */
fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[])
{
    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Validate state */
    if (!stateIsAllowed(myfmu2->state, STATE_fmi2GetReal)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Illegal call sequence for fmi2GetReal.");
        return fmi2Error;
    }

    /* Validate value reference */
    if (nvr > 0 && (!vr || !value)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Invalid value reference or value.");
        return fmi2Error;
    }

    /* Get values */
    for (size_t k = 0; k < nvr; k++) {
        fmi2ValueReference VR = vr[k];

        /* Parameters */
        if (VR < NUMBER_OF_PARAMS) {
            value[k] = myfmu2->model->parameter[VR];
        }
        /* Inputs */
        else if (VR < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS) {
            size_t inputIndex = VR - NUMBER_OF_PARAMS;
            value[k] = myfmu2->model->inputs[inputIndex];
        }
        /* Outputs */
        else if (VR < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS + NUMBER_OF_OUTPUTS) {
            size_t outputIndex = VR - NUMBER_OF_PARAMS - NUMBER_OF_INPUTS;
            value[k] = myfmu2->model->outputs[outputIndex];
        } else {
            myfmu2->state |= stateError;
            logErrorMessage(myfmu2, "The value reference appears to be out of scope for getting values.");
            return fmi2Error;
        }
    }

    logEventMessage(myfmu2, "Values retrieved.");
    return fmi2OK;
}

/**
 * Get integer-valued variables (not implemented - stub)
 */
fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[])
{
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/**
 * Get boolean-valued variables (not implemented - stub)
 */
fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[])
{
    (void)c;
    (void)vr;
    (void)nvr;
    (void)value;
    return fmi2OK;
}

/**
 * Get string-valued variables (not implemented - stub)
 */
fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String value[])
{
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
 * Perform a simulation step: x(t) -> x(t + communicationStepSize)
 *
 * Implementation notes:
 * - Supports variable communication step sizes (not all tools do)
 * - Automatically subdivides large steps into MAX_SOLVER_STEPSIZE increments
 * - Uses sub-stepping for numerical stability
 * - Validates internal clock matches environment clock
 * - Computes outputs after integration via getOutputs()
 *
 * Sub-stepping algorithm:
 * - numberOfSteps = floor(communicationStepSize / MAX_SOLVER_STEPSIZE)
 * - Perform numberOfSteps full-size steps
 * - Handle remainder with smaller final step (if numerically significant)
 */
fmi2Status fmi2DoStep(
    fmi2Component c,
    fmi2Real currentCommunicationPoint,
    fmi2Real communicationStepSize,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint)
{
    /* Unused parameter */
    (void)noSetFMUStatePriorToCurrentPoint;

    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Validate state */
    if (!stateIsAllowed(myfmu2->state, STATE_fmi2DoStep)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Illegal call sequence for fmi2DoStep.");
        return fmi2Error;
    }

    /* Validate communicationStepSize */
    if (communicationStepSize <= 0.0) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "communicationStepSize must be greater than zero.");
        return fmi2Error;
    }

    /* Validate time synchronization */
    if (!areEqualReals(myfmu2->time, currentCommunicationPoint)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "FMU internal clock does not match environment clock.");
        return fmi2Error;
    }

    /* Calculate sub-stepping scheme */
    fmi2Integer numberOfSteps = (fmi2Integer)(communicationStepSize / MAX_SOLVER_STEPSIZE);
    fmi2Real stepRemainder = communicationStepSize - numberOfSteps * MAX_SOLVER_STEPSIZE;
    fmi2Real actualStepSize;

    /* Perform full-size steps */
    actualStepSize = MAX_SOLVER_STEPSIZE;
    for (fmi2Integer k = 0; k < numberOfSteps; k++) {
        myfmu2->model->getStateDerivatives(myfmu2->model);
        myfmu2->model->integrate(myfmu2->model, actualStepSize);
    }

    /* Handle remainder step (if numerically significant) */
    if (stepRemainder >= EPSILON) {
        actualStepSize = stepRemainder;
        myfmu2->model->getStateDerivatives(myfmu2->model);
        myfmu2->model->integrate(myfmu2->model, actualStepSize);
    }

    /* Update internal clock */
    myfmu2->time += communicationStepSize;

    /* Compute outputs */
    myfmu2->model->getOutputs(myfmu2->model);

    logEventMessage(myfmu2, "FMU stepped.");
    return fmi2OK;
}

/**
 * Cancel asynchronous step (not supported)
 */
fmi2Status fmi2CancelStep(fmi2Component c)
{
    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    logWarningMessage(myfmu2, "fmi2CancelStep is not supported.");
    return fmi2Warning;
}

/***************************************************
 * FMI 2.0 Simulation Control
 ***************************************************/

/**
 * Terminate the simulation
 *
 * Signals that the simulation is ending. No resources are freed
 * (that happens in fmi2FreeInstance).
 */
fmi2Status fmi2Terminate(fmi2Component c)
{
    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Validate state */
    if (!stateIsAllowed(myfmu2->state, STATE_fmi2Terminate)) {
        myfmu2->state |= stateError;
        logErrorMessage(myfmu2, "Illegal call sequence for fmi2Terminate.");
        return fmi2Error;
    }

    /* Nothing to cleanup here (done in fmi2FreeInstance) */

    /* Transition state */
    myfmu2->state = stateTerminated;

    logEventMessage(myfmu2, "FMU was terminated.");
    return fmi2OK;
}

/**
 * Reset the FMU to the state right after fmi2Instantiate
 *
 * Per FMI 2.0 spec (section 4.2.4), this brings the FMU back to
 * stateInstantiated so the caller can repeat the full initialization
 * cycle (SetupExperiment → EnterInit → ExitInit → DoStep).
 *
 * Implementation notes:
 * - Resets simulation time to 0.0
 * - Zeros all model arrays (states, derivatives, inputs, outputs, parameters)
 * - Clears function pointers (re-bound during EnterInitializationMode)
 * - Preserves instance identity (name, callbacks, FMU type, log level)
 * - Can be called from any state except stateStartEnd
 */
fmi2Status fmi2Reset(fmi2Component c)
{
    /* Validate component */
    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    /* Reset simulation time */
    myfmu2->startTime = 0.0;
    myfmu2->stopTime  = 0.0;
    myfmu2->time      = 0.0;

    /* Reset model arrays to zero */
    modelInstance* model = myfmu2->model;
    memset(model->states,           0, model->info->numberOfStates    * sizeof(double));
    memset(model->stateDerivatives, 0, model->info->numberOfStates    * sizeof(double));
    memset(model->inputs,           0, model->info->numberOfInputs    * sizeof(double));
    memset(model->outputs,          0, model->info->numberOfOutputs   * sizeof(double));
    memset(model->parameter,        0, model->info->numberOfParameter * sizeof(double));

    /* Clear function pointers (re-bound in fmi2EnterInitializationMode) */
    model->getStateDerivatives = NULL;
    model->integrate           = NULL;
    model->getOutputs          = NULL;

    /* Transition to post-instantiation state */
    myfmu2->state = stateInstantiated;

    logEventMessage(myfmu2, "FMU was reset.");
    return fmi2OK;
}

/***************************************************
 * FMI 2.0 Status Queries (Not Supported)
 ***************************************************/

fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind s, fmi2Status* value)
{
    (void)s;
    (void)value;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2GetStatus is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetRealStatus(fmi2Component c, const fmi2StatusKind s, fmi2Real* value)
{
    (void)s;
    (void)value;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2GetRealStatus is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer* value)
{
    (void)s;
    (void)value;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2GetIntegerStatus is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value)
{
    (void)s;
    (void)value;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2GetBooleanStatus is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetStringStatus(fmi2Component c, const fmi2StatusKind s, fmi2String* value)
{
    (void)s;
    (void)value;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2GetStringStatus is not supported.");
    return fmi2Warning;
}

/***************************************************
 * FMI 2.0 FMU State Management (Not Supported)
 ***************************************************/

fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate)
{
    (void)FMUstate;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2GetFMUstate is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate FMUstate)
{
    (void)FMUstate;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2SetFMUstate is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate)
{
    (void)FMUstate;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2FreeFMUstate is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate FMUstate, size_t* size)
{
    (void)FMUstate;
    (void)size;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2SerializedFMUstateSize is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size)
{
    (void)FMUstate;
    (void)serializedState;
    (void)size;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2SerializeFMUstate is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate)
{
    (void)serializedState;
    (void)size;
    (void)FMUstate;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2DeSerializeFMUstate is not supported.");
    return fmi2Warning;
}

/***************************************************
 * FMI 2.0 Directional Derivatives (Not Supported)
 ***************************************************/

fmi2Status fmi2GetDirectionalDerivative(
    fmi2Component c,
    const fmi2ValueReference vUnknown_ref[],
    size_t nUnknown,
    const fmi2ValueReference vKnown_ref[],
    size_t nKnown,
    const fmi2Real dvKnown[],
    fmi2Real dvUnknown[])
{
    (void)vUnknown_ref;
    (void)nUnknown;
    (void)vKnown_ref;
    (void)nKnown;
    (void)dvKnown;
    (void)dvUnknown;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2GetDirectionalDerivative is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2SetRealInputDerivatives(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Integer order[],
    const fmi2Real value[])
{
    (void)vr;
    (void)nvr;
    (void)order;
    (void)value;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2SetRealInputDerivatives is not supported.");
    return fmi2Warning;
}

fmi2Status fmi2GetRealOutputDerivatives(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Integer order[],
    fmi2Real value[])
{
    (void)vr;
    (void)nvr;
    (void)order;
    (void)value;

    if (!c) {
        return fmi2Error;
    }

    myFMU2* myfmu2 = (myFMU2*)c;
    logWarningMessage(myfmu2, "fmi2GetRealOutputDerivatives is not supported.");
    return fmi2Warning;
}

/***************************************************
 * FMI 2.0 Model Exchange Functions (Stubs)
 *
 * These functions are only applicable to Model Exchange
 * FMUs, not Co-Simulation FMUs. All return fmi2Error.
 ***************************************************/

fmi2Status fmi2SetTime(fmi2Component c, fmi2Real time)
{
    (void)c;
    (void)time;
    return fmi2Error;
}

fmi2Status fmi2SetContinuousStates(fmi2Component c, const fmi2Real x[], size_t nx)
{
    (void)c;
    (void)x;
    (void)nx;
    return fmi2Error;
}

fmi2Status fmi2GetDerivatives(fmi2Component c, fmi2Real derivatives[], size_t nx)
{
    (void)c;
    (void)derivatives;
    (void)nx;
    return fmi2Error;
}

fmi2Status fmi2GetEventIndicators(fmi2Component c, fmi2Real eventIndicators[], size_t ni)
{
    (void)c;
    (void)eventIndicators;
    (void)ni;
    return fmi2Error;
}

fmi2Status fmi2GetContinuousStates(fmi2Component c, fmi2Real x[], size_t nx)
{
    (void)c;
    (void)x;
    (void)nx;
    return fmi2Error;
}

fmi2Status fmi2GetNominalsOfContinuousStates(fmi2Component c, fmi2Real x_nominal[], size_t nx)
{
    (void)c;
    (void)x_nominal;
    (void)nx;
    return fmi2Error;
}

fmi2Status fmi2CompletedIntegratorStep(
    fmi2Component c,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint,
    fmi2Boolean* enterEventMode,
    fmi2Boolean* terminateSimulation)
{
    (void)c;
    (void)noSetFMUStatePriorToCurrentPoint;
    (void)enterEventMode;
    (void)terminateSimulation;
    return fmi2Error;
}

fmi2Status fmi2EnterEventMode(fmi2Component c)
{
    (void)c;
    return fmi2Error;
}

fmi2Status fmi2NewDiscreteStates(fmi2Component c, fmi2EventInfo* eventInfo)
{
    (void)c;
    (void)eventInfo;
    return fmi2Error;
}

fmi2Status fmi2EnterContinuousTimeMode(fmi2Component c)
{
    (void)c;
    return fmi2Error;
}
