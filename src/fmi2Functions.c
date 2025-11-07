
#include <string.h>
#include "fmi2Functions.h"
#include "config.h"
#include "model.h"

/**
 * An instance of the FMU2
 */
typedef struct {
    // TODO : sort by size to minimize padding
    fmi2String instanceName; ///> Name of the FMU
    fmi2Type fmuType;        ///> CS of ME

    modelInstance* model;    ///> Instance of the model
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
 * Create a new instance of a FMU2
 */
fmi2Component fmi2Instantiate(fmi2String instanceName,
                              fmi2Type fmuType,
                              fmi2String fmuGUID,
                              fmi2String fmuResourceLocation,
                              const fmi2CallbackFunctions* functions,
                              fmi2Boolean visible,
                              fmi2Boolean loggingOn) {
    /* compare model guid against modelDescription guid */
    if (strcmp(fmuGUID, GUID) != 0) {
        return NULL;
    }
    /* create an FMU2 instance */
    myFMU2* myfmu2 = malloc(sizeof(myFMU2));
    if (!myfmu2) {
        return NULL;
    }
    /* create a model instance */
    myfmu2->model = newModel(NUMBER_OF_STATES, NUMBER_OF_INPUTS);
    if (!myfmu2->model) {
        free(myfmu2); //TODO: use fmi2FreeInstance()
        return NULL;
    }
    /* fill the fmu with data */
    myfmu2->instanceName = instanceName; // TODO: Maybe us strcpy ?
    myfmu2->fmuType      = fmuType;
    return (fmi2Component)myfmu2;
}

/**
 * Free a FMU2 instance
 */
void fmi2FreeInstance(fmi2Component c) {
    if (c) {
        myFMU2* myfmu2 = (myFMU2*)c;
        freeModel(myfmu2->model);
        free(myfmu2);
    }
}

