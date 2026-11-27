#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <share.h>
#endif

/**
 * Allocates a zero-initialized block of the given size.
 *
 * A block of size 0 is valid (e.g. a model without inputs) and is
 * represented by NULL. The return value tells whether the allocation
 * succeeded, since NULL alone cannot distinguish the two cases.
 */
static bool allocateBlock(void** block, size_t size)
{
    if (size == 0) {
        *block = NULL;
        return true;
    }

    *block = calloc(1, size);
    return *block != NULL;
}

/**
 * Implementation notes for newModel():
 * - Uses calloc() for the instance so that freeModel() can clean up a
 *   partially allocated instance (unallocated blocks are NULL)
 * - Blocks of size 0 are not allocated and stay NULL
 * - stepStart is never empty: the model data always contain time and relTol
 * - Start values and function pointers are set by resetModel()
 */
modelInstance* newModel(const modelDefinition* def)
{
    if (!def) {
        return NULL;
    }

    modelInstance* model = calloc(1, sizeof(modelInstance));
    if (!model) {
        return NULL;
    }

    model->def = def;

    /* Allocate and zero-initialize all storage blocks */
    size_t sizeOfStates = def->numberOfStates * sizeof(double);
    if (!allocateBlock(&model->params, def->sizeOfParams) ||
        !allocateBlock(&model->inputs, def->sizeOfInputs) ||
        !allocateBlock(&model->outputs, def->sizeOfOutputs) ||
        !allocateBlock(&model->locals, def->sizeOfLocals) ||
        !allocateBlock(&model->internals, def->sizeOfInternals) ||
        !allocateBlock((void**)&model->states, sizeOfStates) ||
        !allocateBlock((void**)&model->stateDerivatives, sizeOfStates) ||
        !allocateBlock((void**)&model->initialGuess, sizeOfStates) ||
        !allocateBlock((void**)&model->scratch, MODEL_SCRATCH_VECTORS * sizeOfStates) ||
        !allocateBlock(&model->stepStart, modelDataSize(def))) {
        freeModel(model);
        return NULL;
    }

    /* Apply start values and bind function pointers */
    resetModel(model);

    return model;
}

/**
 * Implementation notes for resetModel():
 * - memset() on a NULL block is avoided by checking the block size
 * - Start values are stored as double in the variable table and converted
 *   to the variable's type here
 * - The start value of a string is the text .startString (NULL = ""). It is
 *   written with modelSetString(), so a start value that does not fit (the
 *   build refuses it, but a definition that bypasses the build may have
 *   one) leaves the zeroed field "" instead of writing past it
 */
void resetModel(modelInstance* model)
{
    if (!model) {
        return;
    }

    const modelDefinition* def = model->def;
    size_t sizeOfStates = def->numberOfStates * sizeof(double);

    /* Every simulation starts at time 0 unless the caller sets another start */
    model->time = 0.0;

    /* The model's suggested tolerance, unless the caller defines another one */
    model->relTol = modelDefaultTolerance(def);

    /* Zero all storage blocks */
    if (def->sizeOfParams > 0) {
        memset(model->params, 0, def->sizeOfParams);
    }
    if (def->sizeOfInputs > 0) {
        memset(model->inputs, 0, def->sizeOfInputs);
    }
    if (def->sizeOfOutputs > 0) {
        memset(model->outputs, 0, def->sizeOfOutputs);
    }
    if (def->sizeOfLocals > 0) {
        memset(model->locals, 0, def->sizeOfLocals);
    }
    if (def->sizeOfInternals > 0) {
        memset(model->internals, 0, def->sizeOfInternals);
    }
    if (sizeOfStates > 0) {
        memset(model->states, 0, sizeOfStates);
        memset(model->stateDerivatives, 0, sizeOfStates);
        memset(model->initialGuess, 0, sizeOfStates);
    }

    /* The next initialization takes new guesses */
    model->hasInitialGuess = false;

    /* Write the start value of every published variable */
    for (size_t k = 0; k < def->numberOfVariables; k++) {
        const modelVariable* variable = &def->variables[k];
        void* address = modelVariableAddress(model, k);

        switch (variable->type) {
            case VAR_REAL:
                *(double*)address = variable->start;
                break;
            case VAR_INTEGER:
                *(int*)address = (int)variable->start;
                break;
            case VAR_BOOLEAN:
                *(bool*)address = (variable->start != 0.0);
                break;
            case VAR_STRING:
                modelSetString(model, k, variable->startString ? variable->startString : "");
                break;
        }
    }

    /* Bind function pointers */
    model->getStateDerivatives = def->getStateDerivatives;
    model->integrate = def->integrate;
    model->getOutputs = def->getOutputs;

    /* No error so far */
    model->message[0] = '\0';
}

double modelDefaultTolerance(const modelDefinition* def)
{
    if (!def || !(def->defaultExperiment.tolerance > 0.0)) {
        return MODEL_DEFAULT_TOLERANCE;
    }

    return def->defaultExperiment.tolerance;
}

bool modelEnumerationHasValue(const modelEnumeration* type, int value)
{
    if (!type) {
        return false;
    }

    for (size_t k = 0; k < type->numberOfItems; k++) {
        if (type->items[k].value == value) {
            return true;
        }
    }
    return false;
}

/**
 * Implementation notes for modelSetString():
 * - The length is counted by a loop that stops after MODEL_STRING_SIZE
 *   bytes, so a value that is far too long (or not terminated within that
 *   range) is never read further than needed to refuse it. strnlen() would
 *   do the same, but it is not part of C11
 * - memmove() instead of memcpy() or strcpy(): value may point into the
 *   variable itself, e.g. if an FMI master passes back the pointer it got
 *   from fmi2GetString
 * - The bytes behind the text are zeroed, so the field only depends on the
 *   text and not on longer texts it held before. Equal model data then
 *   also have equal bytes (e.g. two serialized FMU states)
 * - A string whose field is not MODEL_STRING_SIZE bytes is never written:
 *   the build refuses such a definition, and a definition that bypasses the
 *   build must not make this function write past the field
 */
bool modelSetString(modelInstance* model, size_t index, const char* value)
{
    if (!model || !value || index >= model->def->numberOfVariables) {
        return false;
    }

    const modelVariable* variable = &model->def->variables[index];
    if (variable->type != VAR_STRING || variable->size != MODEL_STRING_SIZE) {
        return false;
    }

    size_t length = 0;
    while (length < MODEL_STRING_SIZE && value[length] != '\0') {
        length++;
    }
    if (length == MODEL_STRING_SIZE) {
        return false;
    }

    char* field = modelVariableAddress(model, index);
    memmove(field, value, length + 1);
    memset(field + length + 1, 0, MODEL_STRING_SIZE - (length + 1));
    return true;
}

void computeCalculatedParameters(modelInstance* model)
{
    if (model && model->def->calculateParameters) {
        model->def->calculateParameters(model);
    }
}

void beginInitialization(modelInstance* model)
{
    if (model) {
        model->hasInitialGuess = false;
    }
}

/**
 * Implementation notes for initializeModel():
 * - The guesses are taken by the first call, not by beginInitialization():
 *   a caller that never calls beginInitialization() (e.g. a standalone
 *   simulation) gets an initialization from the states it set, as before
 * - All states are taken, but only those with .initial = VAR_APPROX are set
 *   back: initialize may only replace these, and the others keep the values
 *   the caller set in the meantime (FMI: states with initial="exact" can
 *   still be set in Initialization Mode)
 * - A call that fails keeps the guesses, so the next call starts from them
 * - The calculated parameters are computed after the guesses are set back
 *   and before initialize, so that the model's initialize function can use
 *   them
 */
simStatus initializeModel(modelInstance* model)
{
    if (!model) {
        return SIM_ERROR;
    }

    const modelDefinition* def = model->def;

    if (!model->hasInitialGuess) {
        if (def->numberOfStates > 0) {
            memcpy(model->initialGuess, model->states, def->numberOfStates * sizeof(double));
        }
        model->hasInitialGuess = true;
    } else {
        for (size_t k = 0; k < def->numberOfVariables; k++) {
            const modelVariable* variable = &def->variables[k];
            if (variable->role == VAR_STATE && variable->initial == VAR_APPROX) {
                size_t index = variable->offset / sizeof(double);
                model->states[index] = model->initialGuess[index];
            }
        }
    }

    computeCalculatedParameters(model);

    if (!model->def->initialize) {
        return SIM_OK;
    }
    return model->def->initialize(model);
}

/**
 * Formats a message into model->message, shared by modelError() and
 * modelDiscard(). vsnprintf() truncates a message that does not fit and
 * always terminates it.
 */
static void formatMessage(modelInstance* model, const char* format, va_list args)
{
    if (model && format) {
        vsnprintf(model->message, sizeof(model->message), format, args);
    }
}

simStatus modelError(modelInstance* model, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    formatMessage(model, format, args);
    va_end(args);
    return SIM_ERROR;
}

simStatus modelDiscard(modelInstance* model, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    formatMessage(model, format, args);
    va_end(args);
    return SIM_DISCARD;
}

/**
 * Implementation notes for modelStateName():
 * - A state is identified by its offset in the state vector, which the
 *   STATE() macros fill in from the struct field
 */
const char* modelStateName(const modelDefinition* def, size_t index)
{
    if (def) {
        for (size_t k = 0; k < def->numberOfVariables; k++) {
            const modelVariable* variable = &def->variables[k];
            if (variable->role == VAR_STATE && variable->offset == index * sizeof(double)) {
                return variable->name;
            }
        }
    }
    return "?";
}

size_t modelFirstNonFiniteState(const modelInstance* model)
{
    size_t k = 0;
    while (k < model->def->numberOfStates && isfinite(model->states[k])) {
        k++;
    }
    return k;
}

size_t modelFirstNonFiniteResult(modelInstance* model)
{
    const modelDefinition* def = model->def;

    for (size_t k = 0; k < def->numberOfVariables; k++) {
        const modelVariable* variable = &def->variables[k];
        if ((variable->role == VAR_OUTPUT || variable->role == VAR_LOCAL) && variable->type == VAR_REAL &&
            !isfinite(*(const double*)modelVariableAddress(model, k))) {
            return k;
        }
    }
    return def->numberOfVariables;
}

const char* modelResultKind(const modelVariable* variable)
{
    return (variable->role == VAR_LOCAL) ? "Local variable" : "Output";
}

/**
 * Implementation notes for modelVariableAddress():
 * - The variable's role selects the storage block
 * - Pointer arithmetic is done on char* because the offset is in bytes
 */
void* modelVariableAddress(modelInstance* model, size_t index)
{
    if (!model || index >= model->def->numberOfVariables) {
        return NULL;
    }

    const modelVariable* variable = &model->def->variables[index];
    char* block = NULL;

    switch (variable->role) {
        case VAR_PARAMETER:
            block = model->params;
            break;
        case VAR_INPUT:
            block = model->inputs;
            break;
        case VAR_OUTPUT:
            block = model->outputs;
            break;
        case VAR_STATE:
            block = (char*)model->states;
            break;
        case VAR_LOCAL:
            block = model->locals;
            break;
        case VAR_CALCULATED_PARAMETER:
            block = model->params;
            break;
    }

    return block + variable->offset;
}

/**
 * Copies one storage block and returns the position behind it.
 *
 * Blocks of size 0 are NULL and are skipped (memcpy() must not be called
 * with NULL, not even for 0 bytes).
 */
static char* copyBlock(char* position, void* block, size_t size, bool save)
{
    if (size > 0) {
        if (save) {
            memcpy(position, block, size);
        } else {
            memcpy(block, position, size);
        }
    }
    return position + size;
}

/**
 * Copies all storage blocks to (save) or from (restore) a buffer.
 *
 * Using one function for both directions guarantees that the blocks are
 * written and read in the same order. New blocks are appended at the end,
 * so a model that does not use them keeps the same buffer layout.
 */
static void copyModelData(modelInstance* model, char* buffer, bool save)
{
    const modelDefinition* def = model->def;
    size_t sizeOfStates = def->numberOfStates * sizeof(double);
    char* position = buffer;

    position = copyBlock(position, &model->time, sizeof(double), save);
    position = copyBlock(position, &model->relTol, sizeof(double), save);
    position = copyBlock(position, model->params, def->sizeOfParams, save);
    position = copyBlock(position, model->inputs, def->sizeOfInputs, save);
    position = copyBlock(position, model->outputs, def->sizeOfOutputs, save);
    position = copyBlock(position, model->states, sizeOfStates, save);
    position = copyBlock(position, model->stateDerivatives, sizeOfStates, save);
    position = copyBlock(position, model->internals, def->sizeOfInternals, save);
    position = copyBlock(position, model->locals, def->sizeOfLocals, save);
    position = copyBlock(position, model->initialGuess, sizeOfStates, save);
    copyBlock(position, &model->hasInitialGuess, sizeof(bool), save);
}

size_t modelDataSize(const modelDefinition* def)
{
    if (!def) {
        return 0;
    }

    /* states, state derivatives and initial guesses: 3 vectors of nx doubles */
    return 2 * sizeof(double) + def->sizeOfParams + def->sizeOfInputs + def->sizeOfOutputs +
           3 * def->numberOfStates * sizeof(double) + def->sizeOfInternals + def->sizeOfLocals + sizeof(bool);
}

/**
 * Implementation notes for saveModelData():
 * - The const is cast away only to share copyModelData() with
 *   restoreModelData(); the model is not modified when saving
 */
void saveModelData(const modelInstance* model, void* buffer)
{
    if (!model || !buffer) {
        return;
    }

    copyModelData((modelInstance*)model, buffer, true);
}

void restoreModelData(modelInstance* model, const void* buffer)
{
    if (!model || !buffer) {
        return;
    }

    copyModelData(model, (char*)buffer, false);
}

/**
 * Implementation notes for modelSetResourcePath():
 * - The new path is copied before the old one is freed, so path may be the
 *   current model->resourcePath
 * - Only a trailing '/' counts as separator on POSIX, where a backslash is
 *   an ordinary character of a file name
 */
bool modelSetResourcePath(modelInstance* model, const char* path)
{
    if (!model) {
        return false;
    }

    char* copy = NULL;
    if (path && path[0] != '\0') {
        size_t length = strlen(path);
        bool endsWithSeparator = path[length - 1] == '/';
#ifdef _WIN32
        endsWithSeparator = endsWithSeparator || path[length - 1] == '\\';
#endif
        copy = malloc(length + (endsWithSeparator ? 1 : 2));
        if (!copy) {
            free(model->resourcePath);
            model->resourcePath = NULL;
            return false;
        }
        memcpy(copy, path, length);
        if (!endsWithSeparator) {
            copy[length++] = '/';
        }
        copy[length] = '\0';
    }

    free(model->resourcePath);
    model->resourcePath = copy;
    return true;
}

#ifdef _WIN32
/**
 * Converts a UTF-8 text to UTF-16 for the wide-character API of Windows.
 * Returns a buffer to free(), or NULL if the text is not valid UTF-8 or
 * memory allocation fails.
 */
static wchar_t* toWide(const char* text)
{
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (length <= 0) {
        return NULL;
    }

    wchar_t* wide = malloc((size_t)length * sizeof(wchar_t));
    if (wide && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, length) != length) {
        free(wide);
        wide = NULL;
    }
    return wide;
}
#endif

/**
 * Implementation notes for modelOpenFile():
 * - POSIX file systems take the path as bytes, so fopen() opens a UTF-8
 *   path as it is
 * - Windows interprets the path of fopen() in the local code page, so the
 *   path is converted to UTF-16 and opened with the wide-character API.
 *   _wfsopen() with _SH_DENYNO shares the file like fopen() does (unlike
 *   _wfopen_s()) and is not deprecated (unlike _wfopen())
 */
FILE* modelOpenFile(const char* path, const char* mode)
{
    if (!path || !mode) {
        return NULL;
    }

#ifdef _WIN32
    wchar_t* widePath = toWide(path);
    wchar_t* wideMode = toWide(mode);
    FILE* file = (widePath && wideMode) ? _wfsopen(widePath, wideMode, _SH_DENYNO) : NULL;
    free(widePath);
    free(wideMode);
    return file;
#else
    return fopen(path, mode);
#endif
}

/**
 * Implementation notes for freeModel():
 * - free(NULL) is safe, so blocks of size 0 need no special handling
 * - Order of freeing doesn't matter as there are no pointer dependencies
 * - Always frees the model instance itself last
 */
void freeModel(modelInstance* model)
{
    if (!model) {
        return;
    }

    /* Free all dynamically allocated members */
    free(model->params);
    free(model->inputs);
    free(model->outputs);
    free(model->locals);
    free(model->internals);
    free(model->states);
    free(model->stateDerivatives);
    free(model->initialGuess);
    free(model->scratch);
    free(model->stepStart);
    free(model->resourcePath);

    /* Free the model instance itself */
    free(model);
}
