#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fmi2Functions.h"
#include "model.h"

/* Relative tolerance for comparing points in time */
#define TIME_TOLERANCE 1e-9

/* A clock difference below this fraction of the communication step size is rounding
   and passes silently; a larger one (still within TIME_TOLERANCE) is reported */
#define CLOCK_WARNING_FRACTION 1e-6

/* Allowed deviation of model->time from t + h after an integrator step,
   as a fraction of the step size h */
#define INTEGRATOR_TIME_FRACTION 1e-6

/* Tolerance on the number of sub-steps (absorbs rounding in stepSize / maxStepSize) */
#define SUBSTEP_TOLERANCE 1e-9

/* Largest number of sub-steps a single fmi2DoStep may take */
#define MAX_NUMBER_OF_SUBSTEPS 1e9

/* Buffer size for formatted log messages */
#define LOG_BUFFER_SIZE 512

/* Marks a block of memory as an FMU state of this template ("FMUS") */
#define SNAPSHOT_MAGIC 0x464D5553u

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
    char* instanceName;

    /* Experiment time bounds (the current time is model->time) */
    fmi2Real    startTime;
    fmi2Real    stopTime;
    fmi2Boolean stopTimeDefined;

    /* Logging configuration */
    unsigned int logLevel;

    /* FMU state machine tracking */
    unsigned int state;

    /* Outputs must be recomputed before they are read */
    fmi2Boolean outputsAreStale;

    /* Integration tool callbacks */
    fmi2CallbackLogger       logger;
    fmi2ComponentEnvironment componentEnvironment;

    /* Model instance (FMU-agnostic) */
    modelInstance* model;
} myFMU2;

/**
 * Log category enumeration
 *
 * Bitmask-based log levels. Multiple categories can be enabled
 * simultaneously via bitwise OR.
 */
typedef enum {
    logNone          = 0,
    logEvents        = 1 << 0,  /* Lifecycle of the FMU (instantiated, initialized, ...) */
    logStatusWarning = 1 << 1,  /* A function returned fmi2Warning or fmi2Discard */
    logStatusError   = 1 << 2,  /* A function returned fmi2Error */
    logFmi2Call      = 1 << 3,  /* Successful calls of frequently used functions */
    logAll           = (1 << 4) - 1
} logCategory;

/**
 * Names of the log categories
 *
 * Single place that maps category names to bits. The same names must be
 * listed as LogCategories in the modelDescription.xml
 * (see fmi2ModelDescription.c).
 */
static const struct {
    const char*  name;
    unsigned int mask;
} LOG_CATEGORIES[] = {
    { "logAll",           logAll },
    { "logEvents",        logEvents },
    { "logStatusWarning", logStatusWarning },
    { "logStatusError",   logStatusError },
    { "logFmi2Call",      logFmi2Call },
};

#define NUMBER_OF_LOG_CATEGORIES (sizeof(LOG_CATEGORIES) / sizeof(LOG_CATEGORIES[0]))

/**
 * FMU state machine enumeration
 *
 * The FMU is in exactly one of these states. They are bits so that the set
 * of states a function may be called in can be written as a bitmask.
 * Follows FMI 2.0 state machine specification for co-simulation.
 */
typedef enum {
    stateStartEnd           = 0,       /* Pseudo-state (not tracked) */
    stateInstantiated       = 1 << 0,  /* Set by fmi2Instantiate and fmi2Reset */
    stateInitializationMode = 1 << 1,  /* Set by fmi2EnterInitializationMode */
    stateStepComplete       = 1 << 2,  /* Set by fmi2ExitInitializationMode and fmi2DoStep */
    stateStepInProgress     = 1 << 3,  /* Asynchronous fmi2DoStep (not used) */
    stateStepFailed         = 1 << 4,  /* fmi2DoStep returned fmi2Discard */
    stateStepCanceled       = 1 << 5,  /* Set by fmi2CancelStep (not used) */
    stateTerminated         = 1 << 6,  /* Set by fmi2Terminate */
    stateError              = 1 << 7,  /* A function returned fmi2Error */
} stateCategory;

/**
 * Snapshot of an FMU instance (fmi2FMUstate)
 *
 * Holds everything that changes during a simulation. Instance name,
 * callbacks and log level belong to the instance and are not part of it.
 * The struct contains no pointers, so its bytes are also the serialized
 * form of the FMU state. The current time and the model's unpublished
 * internals are part of the model data.
 */
typedef struct {
    uint32_t      magic;     /* SNAPSHOT_MAGIC */
    uint32_t      guidHash;  /* Identifies the model the snapshot was taken from */
    fmi2Real      startTime;
    fmi2Real      stopTime;
    fmi2Boolean   stopTimeDefined;
    fmi2Boolean   outputsAreStale;
    unsigned int  state;
    unsigned char modelData[];  /* Filled by saveModelData() */
} fmuSnapshot;

/***************************************************
 * FMI2 Function State Validation Bitmasks
 *
 * These macros define which FMU states are valid
 * for calling each FMI function. Used for runtime
 * state machine validation.
 *
 * Once a function has returned fmi2Error the FMU is in stateError. From
 * there it can only be reset, freed or set back to a snapshot; reading
 * variables and changing the logging stay possible for diagnosis.
 ***************************************************/

#define STATE_anyButError (stateInstantiated | stateInitializationMode | stateStepComplete | \
                           stateStepInProgress | stateStepFailed | stateStepCanceled | stateTerminated)
#define STATE_any         (STATE_anyButError | stateError)

#define STATE_fmi2SetupExperiment (stateInstantiated)
#define STATE_fmi2SetDebugLogging (STATE_any)
#define STATE_fmi2EnterInitializationMode (stateInstantiated)
#define STATE_fmi2ExitInitializationMode (stateInitializationMode)
#define STATE_fmi2SetReal (stateInstantiated | stateInitializationMode | stateStepComplete)
#define STATE_fmi2GetReal (stateInstantiated | stateInitializationMode | stateStepComplete | \
                           stateStepFailed | stateStepCanceled | stateTerminated | stateError)
/* Integer, Boolean and String variables follow the same rules as Real variables */
#define STATE_fmi2SetInteger STATE_fmi2SetReal
#define STATE_fmi2SetBoolean STATE_fmi2SetReal
#define STATE_fmi2SetString  STATE_fmi2SetReal
#define STATE_fmi2GetInteger STATE_fmi2GetReal
#define STATE_fmi2GetBoolean STATE_fmi2GetReal
#define STATE_fmi2GetString  STATE_fmi2GetReal
/* Fixed parameters and states may only be set before the simulation starts */
#define STATE_setBeforeSimulation (stateInstantiated | stateInitializationMode)
#define STATE_fmi2DoStep (stateStepComplete)
#define STATE_fmi2Terminate (stateStepComplete | stateStepFailed)
#define STATE_fmi2Reset (STATE_any)
#define STATE_fmi2GetStatus (stateStepComplete | stateStepInProgress | stateStepFailed | stateTerminated)
/* A snapshot can only be taken from a healthy FMU, but it can be restored
 * to recover from an error */
#define STATE_fmi2GetFMUstate (STATE_anyButError)
#define STATE_fmi2SetFMUstate (STATE_any)
#define STATE_fmi2FreeFMUstate (STATE_any)
#define STATE_fmi2SerializeFMUstate (STATE_any)

/***************************************************
 * Helper Functions
 ***************************************************/

/**
 * Name of a log category as listed in the modelDescription.xml
 */
static const char* logCategoryName(logCategory category)
{
    for (size_t k = 0; k < NUMBER_OF_LOG_CATEGORIES; k++) {
        if (LOG_CATEGORIES[k].mask == (unsigned int)category) {
            return LOG_CATEGORIES[k].name;
        }
    }
    return "logAll";
}

/**
 * Core logging function (variadic message support)
 *
 * Formats message with vsnprintf and forwards to integration tool's logger.
 *
 * Implementation notes:
 * - The logger of the integration tool is itself printf-like. The formatted
 *   text is therefore passed as argument of "%s" and never as the format,
 *   otherwise a '%' in the text (e.g. in a variable name) would be
 *   interpreted a second time
 * - Messages longer than LOG_BUFFER_SIZE are truncated
 */
static void logMessage(
    const myFMU2* fmu,
    logCategory category,
    fmi2Status status,
    const char* message,
    va_list args)
{
    if (fmu->logLevel & category) {
        char buffer[LOG_BUFFER_SIZE];
        vsnprintf(buffer, sizeof(buffer), message, args);
        fmu->logger(
            fmu->componentEnvironment,
            fmu->instanceName,
            status,
            logCategoryName(category),
            "%s",
            buffer
        );
    }
}

/**
 * Log an event in the lifecycle of the FMU
 */
static void logEventMessage(const myFMU2* fmu, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    logMessage(fmu, logEvents, fmi2OK, message, args);
    va_end(args);
}

/**
 * Log a successful call of a frequently used function
 */
static void logCallMessage(const myFMU2* fmu, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    logMessage(fmu, logFmi2Call, fmi2OK, message, args);
    va_end(args);
}

/**
 * Log a warning message
 */
static void logWarningMessage(const myFMU2* fmu, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    logMessage(fmu, logStatusWarning, fmi2Warning, message, args);
    va_end(args);
}

/**
 * Log an error and put the FMU into the error state
 *
 * Always returns fmi2Error, so that a function can fail with
 * "return enterErrorState(fmu, ...)".
 */
static fmi2Status enterErrorState(myFMU2* fmu, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    logMessage(fmu, logStatusError, fmi2Error, message, args);
    va_end(args);

    fmu->state = stateError;
    return fmi2Error;
}

/**
 * Log why a step was discarded and put the FMU into the step failed state
 *
 * Always returns fmi2Discard, so that fmi2DoStep can fail with
 * "return discardStep(fmu, ...)". The caller must have rolled the model
 * back to the communication point before.
 */
static fmi2Status discardStep(myFMU2* fmu, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    logMessage(fmu, logStatusWarning, fmi2Discard, message, args);
    va_end(args);

    fmu->state = stateStepFailed;
    return fmi2Discard;
}

/**
 * Common checks at the start of every FMI function
 *
 * Returns the FMU instance, or NULL if the component is NULL or the
 * function may not be called in the current state. In the latter case the
 * error is logged and the FMU enters the error state.
 */
static myFMU2* enterFunction(fmi2Component c, unsigned int allowedStates, const char* functionName)
{
    myFMU2* fmu = (myFMU2*)c;

    if (!fmu) {
        return NULL;
    }

    if (!(fmu->state & allowedStates)) {
        enterErrorState(fmu, "Illegal call sequence for %s.", functionName);
        return NULL;
    }

    return fmu;
}

/**
 * Fail a function that belongs to a capability the FMU does not have
 *
 * The capability flags in the modelDescription.xml tell the integration
 * tool not to call these functions, so calling them is an error (and not a
 * warning, which would mean that the call had an effect).
 */
static fmi2Status notSupported(fmi2Component c, const char* functionName)
{
    myFMU2* fmu = (myFMU2*)c;

    if (!fmu) {
        return fmi2Error;
    }

    return enterErrorState(fmu, "%s is not supported.", functionName);
}

/**
 * Tolerance for comparing a point in time with the given one
 */
static fmi2Real timeTolerance(fmi2Real time)
{
    return TIME_TOLERANCE * fmax(1.0, fabs(time));
}

/**
 * Rounding allowance for two values of the time t: a few units in the last place
 */
static fmi2Real roundingTolerance(fmi2Real time)
{
    return 8.0 * DBL_EPSILON * fmax(1.0, fabs(time));
}

/**
 * Published name of the state with the given index (for log messages)
 */
static const char* stateName(const modelDefinition* def, size_t index)
{
    for (size_t k = 0; k < def->numberOfVariables; k++) {
        const modelVariable* variable = &def->variables[k];
        if (variable->role == VAR_STATE && variable->offset == index * sizeof(double)) {
            return variable->name;
        }
    }
    return "?";
}

/**
 * Index of the first state that is NaN or infinite, or numberOfStates if
 * all states are finite
 */
static size_t firstNonFiniteState(const modelInstance* model)
{
    size_t k = 0;
    while (k < model->def->numberOfStates && isfinite(model->states[k])) {
        k++;
    }
    return k;
}

/**
 * Index (in the variable table) of the first Real output that is NaN or
 * infinite, or numberOfVariables if all Real outputs are finite
 */
static size_t firstNonFiniteOutput(modelInstance* model)
{
    const modelDefinition* def = model->def;

    for (size_t k = 0; k < def->numberOfVariables; k++) {
        const modelVariable* variable = &def->variables[k];
        if (variable->role == VAR_OUTPUT && variable->type == VAR_REAL &&
            !isfinite(*(const double*)modelVariableAddress(model, k))) {
            return k;
        }
    }
    return def->numberOfVariables;
}

/**
 * Recompute the outputs if a variable was set since they were computed
 */
static void refreshOutputs(myFMU2* fmu)
{
    if (fmu->outputsAreStale) {
        fmu->model->getOutputs(fmu->model);
        fmu->outputsAreStale = fmi2False;
    }
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
 * Report why fmi2Instantiate failed
 *
 * There is no instance yet, so the logger of the integration tool is called
 * directly and independent of loggingOn.
 */
static fmi2Component instantiationFailed(const fmi2CallbackFunctions* functions, fmi2String instanceName,
                                         const char* message)
{
    functions->logger(functions->componentEnvironment, instanceName ? instanceName : "",
                      fmi2Error, logCategoryName(logStatusError), "%s", message);
    return NULL;
}

/**
 * Creates an instance of the FMU
 *
 * Implementation notes:
 * - Validates the logger callback, instance name, FMU type and GUID before
 *   anything is allocated
 * - Only Co-Simulation is supported
 * - All memory is allocated with calloc() and freed with free(). The memory
 *   callbacks of the integration tool are not used, which is announced by
 *   canNotUseMemoryManagementFunctions="true" in the modelDescription.xml
 * - Creates model instance from MODEL_DEFINITION; all variables hold
 *   their start values afterwards
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

    /* Without a logger nothing can be reported */
    if (!functions || !functions->logger) {
        return NULL;
    }

    /* Validate instance name */
    if (!instanceName || instanceName[0] == '\0') {
        return instantiationFailed(functions, instanceName, "Missing instance name.");
    }

    /* Validate FMU type */
    if (fmuType != fmi2CoSimulation) {
        return instantiationFailed(functions, instanceName, "Only Co-Simulation is supported.");
    }

    /* Validate GUID */
    if (!fmuGUID || strcmp(fmuGUID, MODEL_DEFINITION.guid) != 0) {
        return instantiationFailed(functions, instanceName, "GUID mismatch.");
    }

    /* Allocate FMU instance (zero-initialized, so all pointers are NULL) */
    myFMU2* myfmu2 = calloc(1, sizeof(myFMU2));
    if (!myfmu2) {
        return instantiationFailed(functions, instanceName, "Memory allocation failed.");
    }

    /* Store callbacks */
    myfmu2->logger = functions->logger;
    myfmu2->componentEnvironment = functions->componentEnvironment;

    /* Copy instance name (the caller may free its string after this call) */
    size_t sizeOfName = strlen(instanceName) + 1;
    myfmu2->instanceName = malloc(sizeOfName);
    if (!myfmu2->instanceName) {
        fmi2FreeInstance((fmi2Component)myfmu2);
        return instantiationFailed(functions, instanceName, "Memory allocation failed.");
    }
    memcpy(myfmu2->instanceName, instanceName, sizeOfName);

    /* Create model instance */
    myfmu2->model = newModel(&MODEL_DEFINITION);
    if (!myfmu2->model) {
        fmi2FreeInstance((fmi2Component)myfmu2);
        return instantiationFailed(functions, instanceName, "Memory allocation failed.");
    }

    /* Outputs are computed when they are read for the first time */
    myfmu2->outputsAreStale = fmi2True;

    /* Initialize logging */
    myfmu2->logLevel = loggingOn ? logAll : logNone;

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
 * - Can be called in any state, also on a partially created instance
 * - Logs farewell message before cleanup
 * - Frees model via freeModel()
 * - Snapshots taken with fmi2GetFMUstate are not freed here; they belong to
 *   the integration tool until it calls fmi2FreeFMUstate
 */
void fmi2FreeInstance(fmi2Component c)
{
    if (!c) {
        return;
    }

    myFMU2* myfmu2 = (myFMU2*)c;

    logEventMessage(myfmu2, "FMU will be freed from memory.");

    free(myfmu2->instanceName);
    freeModel(myfmu2->model);
    free(myfmu2);
}

/***************************************************
 * FMI 2.0 Setup and Initialization
 ***************************************************/

/**
 * Set up experiment time bounds and tolerance
 *
 * Implementation notes:
 * - A defined tolerance becomes the model's relative tolerance
 *   (model->relTol), otherwise the model's default applies (see
 *   modelDefaultTolerance()). Only adaptive integrators use it; the
 *   fixed-step integrators ignore it
 * - A defined tolerance that is not finite and > 0 is ignored with
 *   fmi2Warning; the default applies
 * - Validates stopTime >= startTime if defined
 * - If a stop time is defined, fmi2DoStep refuses to step beyond it
 * - Initializes the internal clock (the model's time) to startTime
 * - Marks the outputs as stale: h(t, x, u) may depend on the time, so they
 *   are recomputed with the new time when they are read next
 */
fmi2Status fmi2SetupExperiment(
    fmi2Component c,
    fmi2Boolean toleranceDefined,
    fmi2Real tolerance,
    fmi2Real startTime,
    fmi2Boolean stopTimeDefined,
    fmi2Real stopTime)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2SetupExperiment, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    /* Validate time window */
    if (!isfinite(startTime)) {
        return enterErrorState(myfmu2, "startTime must be finite.");
    }
    if (stopTimeDefined && !(stopTime >= startTime)) {
        return enterErrorState(myfmu2, "stopTime %.16g must be >= startTime %.16g.", stopTime, startTime);
    }

    /* A tolerance that is not finite and > 0 is ignored with a warning. FMI 2.0
       defines no range for it and says "An FMU for Co-Simulation might ignore
       this argument" (fmi2SetupExperiment), so it is no reason to fail */
    fmi2Status status = fmi2OK;
    if (toleranceDefined && !(isfinite(tolerance) && tolerance > 0.0)) {
        logWarningMessage(myfmu2, "tolerance %.16g is ignored: it must be finite and greater than zero. "
                                  "The default tolerance applies.", tolerance);
        toleranceDefined = fmi2False;
        status = fmi2Warning;
    }

    /* Store experiment parameters */
    myfmu2->startTime = startTime;
    myfmu2->stopTimeDefined = stopTimeDefined ? fmi2True : fmi2False;
    myfmu2->stopTime = stopTimeDefined ? stopTime : 0.0;
    myfmu2->model->time = startTime;
    myfmu2->model->relTol = toleranceDefined ? tolerance : modelDefaultTolerance(myfmu2->model->def);

    /* The time changed, and h(t, x, u) may depend on it */
    myfmu2->outputsAreStale = fmi2True;

    const char* toleranceSource = toleranceDefined ? "set by the integration tool" : "default";
    if (stopTimeDefined) {
        logEventMessage(myfmu2, "FMU Experiment set up. Start = %.4f. End = %.4f. Tolerance = %g (%s).",
                        startTime, stopTime, myfmu2->model->relTol, toleranceSource);
    } else {
        logEventMessage(myfmu2, "FMU Experiment set up. Start = %.4f. No end defined. Tolerance = %g (%s).",
                        startTime, myfmu2->model->relTol, toleranceSource);
    }
    return status;
}

/**
 * Control which log categories are enabled
 *
 * Categories of this FMU (see LOG_CATEGORIES):
 * - logEvents: lifecycle of the FMU
 * - logStatusWarning: a function returned fmi2Warning or fmi2Discard
 * - logStatusError: a function returned fmi2Error
 * - logFmi2Call: successful calls of fmi2GetXXX, fmi2SetXXX and fmi2DoStep
 * - logAll: all of the above
 *
 * Implementation notes:
 * - If loggingOn=false, disables all logging
 * - If nCategories=0, enables all categories
 * - Otherwise, enables only specified categories
 * - Unknown categories are ignored, but reported with fmi2Warning
 */
fmi2Status fmi2SetDebugLogging(
    fmi2Component c,
    fmi2Boolean loggingOn,
    size_t nCategories,
    const fmi2String categories[])
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2SetDebugLogging, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    /* Validate categories array */
    if (loggingOn && nCategories > 0 && categories == NULL) {
        return enterErrorState(myfmu2, "Invalid log categories.");
    }

    /* Disable all logging if loggingOn=false */
    if (!loggingOn) {
        logEventMessage(myfmu2, "Logging switched off.");
        myfmu2->logLevel = logNone;
        return fmi2OK;
    }

    /* Enable all categories if nCategories=0 */
    if (nCategories == 0) {
        myfmu2->logLevel = logAll;
        logEventMessage(myfmu2, "Log level set.");
        return fmi2OK;
    }

    /* Enable specified categories */
    unsigned int logLevel = logNone;
    size_t numberOfUnknownCategories = 0;

    for (size_t i = 0; i < nCategories; i++) {
        fmi2Boolean known = fmi2False;

        for (size_t k = 0; categories[i] && k < NUMBER_OF_LOG_CATEGORIES; k++) {
            if (strcmp(categories[i], LOG_CATEGORIES[k].name) == 0) {
                logLevel |= LOG_CATEGORIES[k].mask;
                known = fmi2True;
                break;
            }
        }

        if (!known) {
            numberOfUnknownCategories++;
        }
    }

    myfmu2->logLevel = logLevel;

    if (numberOfUnknownCategories > 0) {
        logWarningMessage(myfmu2, "%zu unknown log categories were ignored.", numberOfUnknownCategories);
        return fmi2Warning;
    }

    logEventMessage(myfmu2, "Log level set.");
    return fmi2OK;
}

/**
 * Enter initialization mode
 *
 * Implementation notes:
 * - Does not touch any variable: start values were written by
 *   fmi2Instantiate (or fmi2Reset), so values the integration tool has set
 *   since then are kept
 * - The integration tool can keep overriding parameters and states until
 *   fmi2ExitInitializationMode
 * - A missing fmi2SetupExperiment is tolerated: the experiment then starts
 *   at time 0 and has no stop time
 */
fmi2Status fmi2EnterInitializationMode(fmi2Component c)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2EnterInitializationMode, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    /* Update state */
    myfmu2->state = stateInitializationMode;

    logEventMessage(myfmu2, "FMU entered initialization mode.");
    return fmi2OK;
}

/**
 * Exit initialization mode and finalize setup
 *
 * Transitions to step mode, ready for simulation.
 *
 * Implementation notes:
 * - Computes the outputs from the initial states, inputs and parameters,
 *   so that outputs are valid at the first communication point
 */
fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2ExitInitializationMode, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    /* Compute initial outputs */
    myfmu2->model->getOutputs(myfmu2->model);
    myfmu2->outputsAreStale = fmi2False;

    /* Transition state */
    myfmu2->state = stateStepComplete;

    logEventMessage(myfmu2, "FMU initialized.");
    return fmi2OK;
}

/***************************************************
 * FMI 2.0 Getting and Setting Variables
 *
 * The value reference of a variable is its index in the variable table of
 * the model definition (the same mapping is used to generate
 * modelDescription.xml). The table tells type, role and storage location,
 * so none of the functions below knows anything about the model.
 *
 * Outputs are kept consistent with the other variables: every fmi2SetXXX
 * (and fmi2SetupExperiment, which moves the time) marks them as stale and
 * every fmi2GetXXX recomputes them if needed. This
 * way outputs can be read in initialization mode and a model with direct
 * feedthrough (y depends on u) answers with the inputs set last.
 ***************************************************/

/**
 * Name of a variable type for log messages
 */
static const char* typeName(varType type)
{
    switch (type) {
        case VAR_REAL:    return "Real";
        case VAR_INTEGER: return "Integer";
        case VAR_BOOLEAN: return "Boolean";
    }
    return "unknown";
}

/**
 * Common checks at the start of every fmi2GetXXX / fmi2SetXXX function
 *
 * Validates the component, the FMU state and the argument arrays. Returns
 * the FMU instance or NULL on failure (see enterFunction()).
 */
static myFMU2* enterVariableAccess(
    fmi2Component c,
    unsigned int allowedStates,
    const char* functionName,
    const fmi2ValueReference vr[],
    size_t nvr,
    const void* value)
{
    myFMU2* fmu = enterFunction(c, allowedStates, functionName);
    if (!fmu) {
        return NULL;
    }

    /* Validate argument arrays */
    if (nvr > 0 && (!vr || !value)) {
        enterErrorState(fmu, "%s: invalid value reference or value.", functionName);
        return NULL;
    }

    return fmu;
}

/**
 * Map a value reference to the storage of a model variable
 *
 * Checks that the value reference exists, that the variable has the
 * expected type and, for write access, that the variable may be set in the
 * current FMU state:
 * - Outputs can never be set
 * - States and fixed parameters can only be set before the simulation
 *   starts (until fmi2ExitInitializationMode)
 * - Inputs and tunable parameters can be set whenever setting is allowed
 *
 * Returns NULL on failure. The error is logged and the FMU enters the
 * error state.
 */
static void* resolveVariable(myFMU2* fmu, fmi2ValueReference vr, varType expectedType, fmi2Boolean forWrite)
{
    const modelDefinition* def = fmu->model->def;

    if (vr >= def->numberOfVariables) {
        enterErrorState(fmu, "Value reference %u does not exist.", vr);
        return NULL;
    }

    const modelVariable* variable = &def->variables[vr];

    if (variable->type != expectedType) {
        enterErrorState(fmu, "Variable '%s' (value reference %u) is of type %s, not %s.",
                        variable->name, vr, typeName(variable->type), typeName(expectedType));
        return NULL;
    }

    if (forWrite) {
        if (variable->role == VAR_OUTPUT) {
            enterErrorState(fmu, "Variable '%s' is an output and cannot be set.", variable->name);
            return NULL;
        }

        fmi2Boolean onlyBeforeSimulation =
            variable->role == VAR_STATE ||
            (variable->role == VAR_PARAMETER && variable->variability == VAR_FIXED);

        if (onlyBeforeSimulation && !(fmu->state & STATE_setBeforeSimulation)) {
            enterErrorState(fmu, "Variable '%s' can only be set before the simulation starts.", variable->name);
            return NULL;
        }
    }

    return modelVariableAddress(fmu->model, vr);
}

/**
 * Set real-valued variables
 *
 * Implementation notes:
 * - Looks up each value reference in the model's variable table
 * - See resolveVariable() for which variables can be set when
 */
fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[])
{
    myFMU2* myfmu2 = enterVariableAccess(c, STATE_fmi2SetReal, __func__, vr, nvr, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    /* Set values */
    for (size_t k = 0; k < nvr; k++) {
        double* variable = resolveVariable(myfmu2, vr[k], VAR_REAL, fmi2True);
        if (!variable) {
            return fmi2Error;
        }
        *variable = value[k];
    }

    myfmu2->outputsAreStale = fmi2True;

    logCallMessage(myfmu2, "Values set.");
    return fmi2OK;
}

/**
 * Set integer-valued variables
 */
fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[])
{
    myFMU2* myfmu2 = enterVariableAccess(c, STATE_fmi2SetInteger, __func__, vr, nvr, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    /* Set values */
    for (size_t k = 0; k < nvr; k++) {
        int* variable = resolveVariable(myfmu2, vr[k], VAR_INTEGER, fmi2True);
        if (!variable) {
            return fmi2Error;
        }
        *variable = value[k];
    }

    myfmu2->outputsAreStale = fmi2True;

    logCallMessage(myfmu2, "Values set.");
    return fmi2OK;
}

/**
 * Set boolean-valued variables
 *
 * Implementation notes:
 * - fmi2Boolean is an int (fmi2True / fmi2False), the model stores a bool
 */
fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[])
{
    myFMU2* myfmu2 = enterVariableAccess(c, STATE_fmi2SetBoolean, __func__, vr, nvr, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    /* Set values */
    for (size_t k = 0; k < nvr; k++) {
        bool* variable = resolveVariable(myfmu2, vr[k], VAR_BOOLEAN, fmi2True);
        if (!variable) {
            return fmi2Error;
        }
        *variable = (value[k] != fmi2False);
    }

    myfmu2->outputsAreStale = fmi2True;

    logCallMessage(myfmu2, "Values set.");
    return fmi2OK;
}

/**
 * Set string-valued variables
 *
 * The template does not support string variables, so every value
 * reference is invalid for this function.
 */
fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[])
{
    myFMU2* myfmu2 = enterVariableAccess(c, STATE_fmi2SetString, __func__, vr, nvr, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (nvr > 0) {
        return enterErrorState(myfmu2, "Value reference %u is not a String variable (strings are not supported).",
                               vr[0]);
    }

    return fmi2OK;
}

/**
 * Get real-valued variables
 *
 * Implementation notes:
 * - Looks up each value reference in the model's variable table
 * - Every variable (parameter, input, output, state) can be read
 * - Outputs are recomputed first if a variable was set since the last
 *   computation
 */
fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[])
{
    myFMU2* myfmu2 = enterVariableAccess(c, STATE_fmi2GetReal, __func__, vr, nvr, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    refreshOutputs(myfmu2);

    /* Get values */
    for (size_t k = 0; k < nvr; k++) {
        const double* variable = resolveVariable(myfmu2, vr[k], VAR_REAL, fmi2False);
        if (!variable) {
            return fmi2Error;
        }
        value[k] = *variable;
    }

    logCallMessage(myfmu2, "Values retrieved.");
    return fmi2OK;
}

/**
 * Get integer-valued variables
 */
fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[])
{
    myFMU2* myfmu2 = enterVariableAccess(c, STATE_fmi2GetInteger, __func__, vr, nvr, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    refreshOutputs(myfmu2);

    /* Get values */
    for (size_t k = 0; k < nvr; k++) {
        const int* variable = resolveVariable(myfmu2, vr[k], VAR_INTEGER, fmi2False);
        if (!variable) {
            return fmi2Error;
        }
        value[k] = *variable;
    }

    logCallMessage(myfmu2, "Values retrieved.");
    return fmi2OK;
}

/**
 * Get boolean-valued variables
 */
fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[])
{
    myFMU2* myfmu2 = enterVariableAccess(c, STATE_fmi2GetBoolean, __func__, vr, nvr, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    refreshOutputs(myfmu2);

    /* Get values */
    for (size_t k = 0; k < nvr; k++) {
        const bool* variable = resolveVariable(myfmu2, vr[k], VAR_BOOLEAN, fmi2False);
        if (!variable) {
            return fmi2Error;
        }
        value[k] = *variable ? fmi2True : fmi2False;
    }

    logCallMessage(myfmu2, "Values retrieved.");
    return fmi2OK;
}

/**
 * Get string-valued variables
 *
 * The template does not support string variables, so every value
 * reference is invalid for this function.
 */
fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String value[])
{
    myFMU2* myfmu2 = enterVariableAccess(c, STATE_fmi2GetString, __func__, vr, nvr, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (nvr > 0) {
        return enterErrorState(myfmu2, "Value reference %u is not a String variable (strings are not supported).",
                               vr[0]);
    }

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
 * - Validates that the internal clock matches the environment clock. A
 *   difference up to rounding (roundingTolerance()) or up to
 *   CLOCK_WARNING_FRACTION of the step size passes silently; a larger one
 *   up to timeTolerance() is accepted with fmi2Warning, and the FMU continues
 *   from currentCommunicationPoint; beyond that the call fails with fmi2Error
 * - Refuses to step beyond the stop time of the experiment (if defined)
 * - Inputs are held constant during the step
 * - The model owns the time: model->time is the internal clock
 * - Enforces the integrator contract (integrateFn in model.h): after every
 *   sub-step model->time must be its end, up to INTEGRATOR_TIME_FRACTION of
 *   the sub-step or rounding (roundingTolerance()). Otherwise the integrator
 *   is broken: the function fails with fmi2Error, the model keeps the values
 *   the integrator left, and the outputs are recomputed from them when read
 * - Computes outputs after integration via getOutputs()
 *
 * Numerical failure (FMI 2.0, section 4.2.3: "fmi2Discard ... the slave
 * computed successfully only a subinterval of the communication step"):
 * - The model data are saved to model->stepStart before the first sub-step
 *   (preallocated, no allocation during the step)
 * - After every sub-step the states are checked. If one is NaN or infinite,
 *   the model is restored to currentCommunicationPoint, the function returns
 *   fmi2Discard and the FMU enters stateStepFailed
 * - After the last sub-step the Real outputs are checked the same way. This
 *   also lets a model without states discard a step
 * - Outputs computed outside fmi2DoStep (when read after a set, or in
 *   fmi2ExitInitializationMode) are not checked: FMI 2.0 has no fmi2Discard
 *   for these functions, and the next step discards
 * - The rollback goes to the communication point and not to the last good
 *   sub-step, so fmi2LastSuccessfulTime is currentCommunicationPoint and the
 *   outputs are those of that point
 * - From stateStepFailed the master can read variables and the status, go
 *   back with fmi2SetFMUstate, terminate or reset. fmi2DoStep is refused
 * - fmi2Discard is only used for numerical failure, never to stop at an event
 *
 * Sub-stepping algorithm:
 * - The communication step is divided into the smallest number of equal
 *   sub-steps that are not larger than the model's maxStepSize:
 *   numberOfSteps = ceil(communicationStepSize / maxStepSize)
 * - SUBSTEP_TOLERANCE keeps rounding in the division from adding a step
 *   (0.3 / 0.1 must give 3 steps, not 4)
 * - Every sub-step starts at currentCommunicationPoint + k * actualStepSize,
 *   computed from the integer k, so rounding errors do not accumulate over
 *   the sub-steps
 * - The internal clock is set to the end of the step as seen by the
 *   integration tool, so rounding errors do not accumulate over the steps
 *   either, and the outputs are evaluated exactly at the communication point
 */
fmi2Status fmi2DoStep(
    fmi2Component c,
    fmi2Real currentCommunicationPoint,
    fmi2Real communicationStepSize,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint)
{
    /* Unused parameter */
    (void)noSetFMUStatePriorToCurrentPoint;

    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2DoStep, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    modelInstance* model = myfmu2->model;

    /* Validate communicationStepSize */
    if (!isfinite(communicationStepSize) || communicationStepSize <= 0.0) {
        return enterErrorState(myfmu2, "communicationStepSize must be finite and greater than zero.");
    }

    /* Validate time synchronization (also fails if currentCommunicationPoint is not finite) */
    if (!(fabs(model->time - currentCommunicationPoint) <= timeTolerance(model->time))) {
        return enterErrorState(myfmu2, "FMU internal clock (%.16g) does not match environment clock (%.16g).",
                               model->time, currentCommunicationPoint);
    }

    /* Validate stop time */
    fmi2Real nextCommunicationPoint = currentCommunicationPoint + communicationStepSize;
    if (myfmu2->stopTimeDefined &&
        nextCommunicationPoint > myfmu2->stopTime + timeTolerance(myfmu2->stopTime)) {
        return enterErrorState(myfmu2, "Step from %.16g to %.16g exceeds the stop time %.16g.",
                               currentCommunicationPoint, nextCommunicationPoint, myfmu2->stopTime);
    }

    /* Calculate sub-stepping scheme */
    fmi2Real numberOfSteps = ceil(communicationStepSize / model->def->maxStepSize - SUBSTEP_TOLERANCE);
    if (numberOfSteps < 1.0) {
        numberOfSteps = 1.0;
    }
    if (numberOfSteps > MAX_NUMBER_OF_SUBSTEPS) {
        return enterErrorState(myfmu2, "communicationStepSize %.16g needs too many integration steps.",
                               communicationStepSize);
    }
    fmi2Real actualStepSize = communicationStepSize / numberOfSteps;

    /* A clock difference beyond rounding is accepted, but reported: the FMU
       continues from the environment's clock. Checked after all arguments are
       validated, so that a rejected call only reports its error */
    fmi2Status status = fmi2OK;
    fmi2Real clockDifference = fabs(model->time - currentCommunicationPoint);
    if (clockDifference > fmax(CLOCK_WARNING_FRACTION * communicationStepSize, roundingTolerance(model->time))) {
        logWarningMessage(myfmu2, "FMU internal clock (%.16g) differs from environment clock (%.16g) by %.3g, "
                                  "%.3g of the step size. The FMU continues from the environment clock.",
                          model->time, currentCommunicationPoint, clockDifference,
                          clockDifference / communicationStepSize);
        status = fmi2Warning;
    }

    /* Remember the communication point to roll back a failed step */
    saveModelData(model, model->stepStart);

    /* Perform sub-steps */
    for (size_t k = 0; k < (size_t)numberOfSteps; k++) {
        fmi2Real subStepStart = currentCommunicationPoint + (fmi2Real)k * actualStepSize;
        model->time = subStepStart;
        model->getStateDerivatives(model);
        model->integrate(model, actualStepSize);

        /* Enforce the integrator contract: on return model->time is t + h. A
         * violation is a programming error, not a numerical failure. The model is
         * left as the integrator left it (for diagnosis); the outputs are marked
         * stale, so that values read in the error state match the other variables */
        fmi2Real expectedTime = subStepStart + actualStepSize;
        if (!(fabs(model->time - expectedTime) <=
              fmax(INTEGRATOR_TIME_FRACTION * actualStepSize, roundingTolerance(expectedTime)))) {
            myfmu2->outputsAreStale = fmi2True;
            return enterErrorState(myfmu2, "The integrator left model->time at %.16g instead of %.16g "
                                           "(t + h). See the integrator contract in include/model.h.",
                                   model->time, expectedTime);
        }

        /* Discard the step if the integration diverged */
        size_t failed = firstNonFiniteState(model);
        if (failed < model->def->numberOfStates) {
            fmi2Real failureTime = model->time;
            restoreModelData(model, model->stepStart);
            return discardStep(myfmu2,
                               "State '%s' is not finite at t = %.16g. Step from %.16g to %.16g discarded, "
                               "FMU rolled back to %.16g.",
                               stateName(model->def, failed), failureTime, currentCommunicationPoint,
                               nextCommunicationPoint, model->time);
        }
    }

    /* Update internal clock */
    model->time = nextCommunicationPoint;

    /* Compute outputs; discard the step if one of them cannot be evaluated.
     * The outputs are marked as up to date only after the check, so that a
     * rollback leaves the output block and the stale flag both at t_k */
    model->getOutputs(model);

    size_t failedOutput = firstNonFiniteOutput(model);
    if (failedOutput < model->def->numberOfVariables) {
        restoreModelData(model, model->stepStart);
        return discardStep(myfmu2,
                           "Output '%s' is not finite at t = %.16g. Step from %.16g to %.16g discarded, "
                           "FMU rolled back to %.16g.",
                           model->def->variables[failedOutput].name, nextCommunicationPoint,
                           currentCommunicationPoint, nextCommunicationPoint, model->time);
    }
    myfmu2->outputsAreStale = fmi2False;

    logCallMessage(myfmu2, "FMU stepped to %.16g.", nextCommunicationPoint);
    return status;
}

/**
 * Cancel asynchronous step (not supported)
 *
 * fmi2DoStep never returns fmi2Pending, so there is never a step to cancel.
 */
fmi2Status fmi2CancelStep(fmi2Component c)
{
    return notSupported(c, __func__);
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
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2Terminate, __func__);
    if (!myfmu2) {
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
 * - Resets the experiment (start time 0.0, no stop time)
 * - Resets the time to 0.0 and all model variables to their start values
 *   (see resetModel())
 * - Preserves instance identity (name, callbacks, log level)
 * - Can be called from any state and is the way out of the error state
 */
fmi2Status fmi2Reset(fmi2Component c)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2Reset, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    /* Reset experiment */
    myfmu2->startTime       = 0.0;
    myfmu2->stopTime        = 0.0;
    myfmu2->stopTimeDefined = fmi2False;

    /* Reset the time and all model variables to their start values */
    resetModel(myfmu2->model);
    myfmu2->outputsAreStale = fmi2True;

    /* Transition to post-instantiation state */
    myfmu2->state = stateInstantiated;

    logEventMessage(myfmu2, "FMU was reset.");
    return fmi2OK;
}

/***************************************************
 * FMI 2.0 Status Queries
 *
 * These functions mainly serve asynchronous slaves and slaves that discard
 * steps. This FMU steps synchronously, so a step is always finished when
 * they can be called. It discards a step on numerical failure (see
 * fmi2DoStep), which they report. A status kind that is not available is
 * answered with fmi2Discard, as required by the standard.
 ***************************************************/

/**
 * Common part of the status queries
 *
 * Returns the FMU instance or NULL on failure (see enterFunction()).
 */
static myFMU2* enterStatusQuery(fmi2Component c, const char* functionName, const void* value)
{
    myFMU2* fmu = enterFunction(c, STATE_fmi2GetStatus, functionName);
    if (!fmu) {
        return NULL;
    }

    if (!value) {
        enterErrorState(fmu, "%s: invalid value.", functionName);
        return NULL;
    }

    return fmu;
}

/**
 * Answer a status query for a kind of status the function does not provide
 */
static fmi2Status statusNotAvailable(const myFMU2* fmu, const char* functionName, fmi2StatusKind s)
{
    logWarningMessage(fmu, "%s: status kind %d is not available.", functionName, (int)s);
    return fmi2Discard;
}

/**
 * Status of the last fmi2DoStep
 *
 * Implementation notes:
 * - fmi2DoStepStatus: steps are computed synchronously, so a step is always
 *   finished when this function can be called. The value is the result of
 *   the last fmi2DoStep: fmi2Discard in stateStepFailed, otherwise fmi2OK.
 *   It is derived from the state machine, so a restored FMU state reports
 *   the status of the step it was taken after
 */
fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind s, fmi2Status* value)
{
    myFMU2* myfmu2 = enterStatusQuery(c, __func__, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (s != fmi2DoStepStatus) {
        return statusNotAvailable(myfmu2, __func__, s);
    }

    *value = (myfmu2->state == stateStepFailed) ? fmi2Discard : fmi2OK;
    return fmi2OK;
}

/**
 * Real-valued status
 *
 * Implementation notes:
 * - fmi2LastSuccessfulTime: end of the last successful step, which is the
 *   internal clock of the FMU. After fmi2DoStep returned fmi2Discard this is
 *   the communication point of the discarded step, because the FMU was
 *   rolled back to it
 */
fmi2Status fmi2GetRealStatus(fmi2Component c, const fmi2StatusKind s, fmi2Real* value)
{
    myFMU2* myfmu2 = enterStatusQuery(c, __func__, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (s != fmi2LastSuccessfulTime) {
        return statusNotAvailable(myfmu2, __func__, s);
    }

    *value = myfmu2->model->time;
    return fmi2OK;
}

/**
 * Integer-valued status (FMI 2.0 defines no status kind of this type)
 */
fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer* value)
{
    myFMU2* myfmu2 = enterStatusQuery(c, __func__, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    return statusNotAvailable(myfmu2, __func__, s);
}

/**
 * Boolean-valued status
 *
 * Implementation notes:
 * - fmi2Terminated: the FMU never asks to stop the simulation. After
 *   fmi2DoStep returned fmi2Discard, fmi2False tells the master that the
 *   step may be repeated (e.g. with a smaller step size) instead of
 *   ending the simulation (FMI 2.0, section 4.2.3)
 */
fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value)
{
    myFMU2* myfmu2 = enterStatusQuery(c, __func__, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (s != fmi2Terminated) {
        return statusNotAvailable(myfmu2, __func__, s);
    }

    *value = fmi2False;
    return fmi2OK;
}

/**
 * String-valued status
 *
 * Implementation notes:
 * - fmi2PendingStatus: no step is ever pending, the answer is an empty string
 */
fmi2Status fmi2GetStringStatus(fmi2Component c, const fmi2StatusKind s, fmi2String* value)
{
    myFMU2* myfmu2 = enterStatusQuery(c, __func__, value);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (s != fmi2PendingStatus) {
        return statusNotAvailable(myfmu2, __func__, s);
    }

    *value = "";
    return fmi2OK;
}

/***************************************************
 * FMI 2.0 FMU State Management
 *
 * A snapshot (fmuSnapshot) is one block of memory without pointers. It is
 * owned by the integration tool, which frees it with fmi2FreeFMUstate.
 * Because of this layout the serialized FMU state is simply a copy of the
 * snapshot's bytes. It can be read back by the same FMU binary, but not on
 * another platform.
 ***************************************************/

/**
 * Size in bytes of a snapshot of the model
 */
static size_t snapshotSize(const myFMU2* fmu)
{
    return sizeof(fmuSnapshot) + modelDataSize(fmu->model->def);
}

/**
 * Hash of the model's GUID (FNV-1a)
 *
 * Stored in every snapshot to recognize serialized states of another model.
 */
static uint32_t guidHash(const myFMU2* fmu)
{
    uint32_t hash = 2166136261u;

    for (const char* c = fmu->model->def->guid; *c != '\0'; c++) {
        hash ^= (unsigned char)*c;
        hash *= 16777619u;
    }

    return hash;
}

/**
 * Check that a block of memory is a snapshot of this model
 */
static fmi2Boolean snapshotIsValid(const myFMU2* fmu, const fmuSnapshot* snapshot)
{
    return snapshot &&
           snapshot->magic == SNAPSHOT_MAGIC &&
           snapshot->guidHash == guidHash(fmu) &&
           (snapshot->state & STATE_anyButError) != 0;
}

/**
 * Take a snapshot of the FMU
 *
 * Implementation notes:
 * - If *FMUstate is NULL a new snapshot is allocated, otherwise the
 *   existing one is overwritten (all snapshots of a model have the same size)
 * - Cannot be called in the error state
 */
fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2GetFMUstate, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (!FMUstate) {
        return enterErrorState(myfmu2, "fmi2GetFMUstate: invalid FMU state.");
    }

    fmuSnapshot* snapshot = *FMUstate;

    if (!snapshot) {
        /* calloc() also zeroes the padding bytes, so that equal snapshots
         * serialize to equal bytes */
        snapshot = calloc(1, snapshotSize(myfmu2));
        if (!snapshot) {
            return enterErrorState(myfmu2, "fmi2GetFMUstate: memory allocation failed.");
        }
    } else if (!snapshotIsValid(myfmu2, snapshot)) {
        return enterErrorState(myfmu2, "fmi2GetFMUstate: invalid FMU state.");
    }

    snapshot->magic           = SNAPSHOT_MAGIC;
    snapshot->guidHash        = guidHash(myfmu2);
    snapshot->startTime       = myfmu2->startTime;
    snapshot->stopTime        = myfmu2->stopTime;
    snapshot->stopTimeDefined = myfmu2->stopTimeDefined;
    snapshot->outputsAreStale = myfmu2->outputsAreStale;
    snapshot->state           = myfmu2->state;
    saveModelData(myfmu2->model, snapshot->modelData);

    *FMUstate = snapshot;

    logEventMessage(myfmu2, "FMU state saved at time %.16g.", myfmu2->model->time);
    return fmi2OK;
}

/**
 * Set the FMU back to a snapshot
 *
 * Implementation notes:
 * - Restores variables, clock, experiment and the state of the FMU state
 *   machine
 * - Can be called after a discarded step (stateStepFailed) and in the error
 *   state: this is the way to continue a simulation after a failed step
 *   without starting over
 */
fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate FMUstate)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2SetFMUstate, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    const fmuSnapshot* snapshot = FMUstate;

    if (!snapshotIsValid(myfmu2, snapshot)) {
        return enterErrorState(myfmu2, "fmi2SetFMUstate: invalid FMU state.");
    }

    myfmu2->startTime       = snapshot->startTime;
    myfmu2->stopTime        = snapshot->stopTime;
    myfmu2->stopTimeDefined = snapshot->stopTimeDefined;
    myfmu2->outputsAreStale = snapshot->outputsAreStale;
    myfmu2->state           = snapshot->state;
    restoreModelData(myfmu2->model, snapshot->modelData);

    logEventMessage(myfmu2, "FMU state restored to time %.16g.", myfmu2->model->time);
    return fmi2OK;
}

/**
 * Free a snapshot
 *
 * Implementation notes:
 * - A NULL snapshot is ignored
 * - Sets *FMUstate to NULL
 * - Can be called in any state
 */
fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2FreeFMUstate, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (FMUstate && *FMUstate) {
        free(*FMUstate);
        *FMUstate = NULL;
    }

    return fmi2OK;
}

/**
 * Number of bytes needed to serialize a snapshot
 */
fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate FMUstate, size_t* size)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2SerializeFMUstate, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (!snapshotIsValid(myfmu2, FMUstate) || !size) {
        return enterErrorState(myfmu2, "fmi2SerializedFMUstateSize: invalid FMU state or size.");
    }

    *size = snapshotSize(myfmu2);
    return fmi2OK;
}

/**
 * Copy a snapshot into a byte vector provided by the integration tool
 */
fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2SerializeFMUstate, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (!snapshotIsValid(myfmu2, FMUstate) || !serializedState) {
        return enterErrorState(myfmu2, "fmi2SerializeFMUstate: invalid FMU state or byte vector.");
    }

    if (size < snapshotSize(myfmu2)) {
        return enterErrorState(myfmu2, "fmi2SerializeFMUstate: byte vector has %zu bytes, %zu are needed.",
                               size, snapshotSize(myfmu2));
    }

    memcpy(serializedState, FMUstate, snapshotSize(myfmu2));
    return fmi2OK;
}

/**
 * Create a snapshot from a byte vector written by fmi2SerializeFMUstate
 *
 * Implementation notes:
 * - The bytes come from outside (e.g. a file), so size, marker, model and
 *   state are verified before the bytes are accepted as a snapshot
 * - The bytes are copied into the snapshot first because the byte vector
 *   need not be aligned for an fmuSnapshot
 * - If *FMUstate is NULL a new snapshot is allocated, otherwise the
 *   existing one is overwritten
 */
fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate)
{
    myFMU2* myfmu2 = enterFunction(c, STATE_fmi2SerializeFMUstate, __func__);
    if (!myfmu2) {
        return fmi2Error;
    }

    if (!serializedState || !FMUstate) {
        return enterErrorState(myfmu2, "fmi2DeSerializeFMUstate: invalid byte vector or FMU state.");
    }

    if (size != snapshotSize(myfmu2)) {
        return enterErrorState(myfmu2, "fmi2DeSerializeFMUstate: byte vector has %zu bytes, %zu are expected.",
                               size, snapshotSize(myfmu2));
    }

    /* Verify the header before touching an existing snapshot */
    fmuSnapshot header;
    memcpy(&header, serializedState, sizeof(header));
    if (!snapshotIsValid(myfmu2, &header)) {
        return enterErrorState(myfmu2, "fmi2DeSerializeFMUstate: bytes are not an FMU state of this model.");
    }

    fmuSnapshot* snapshot = *FMUstate;

    if (!snapshot) {
        snapshot = malloc(size);
        if (!snapshot) {
            return enterErrorState(myfmu2, "fmi2DeSerializeFMUstate: memory allocation failed.");
        }
    } else if (!snapshotIsValid(myfmu2, snapshot)) {
        return enterErrorState(myfmu2, "fmi2DeSerializeFMUstate: invalid FMU state.");
    }

    memcpy(snapshot, serializedState, size);
    *FMUstate = snapshot;

    return fmi2OK;
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

    return notSupported(c, __func__);
}

/***************************************************
 * FMI 2.0 Input and Output Derivatives (Not Supported)
 ***************************************************/

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

    return notSupported(c, __func__);
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

    return notSupported(c, __func__);
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
