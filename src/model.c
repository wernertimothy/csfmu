#include <stdlib.h>
#include <string.h>
#include "model.h"

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
        !allocateBlock((void**)&model->states, sizeOfStates) ||
        !allocateBlock((void**)&model->stateDerivatives, sizeOfStates) ||
        !allocateBlock((void**)&model->scratch, MODEL_SCRATCH_VECTORS * sizeOfStates)) {
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
    if (sizeOfStates > 0) {
        memset(model->states, 0, sizeOfStates);
        memset(model->stateDerivatives, 0, sizeOfStates);
    }

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
        }
    }

    /* Bind function pointers */
    model->getStateDerivatives = def->getStateDerivatives;
    model->integrate = def->integrate;
    model->getOutputs = def->getOutputs;
}

double modelDefaultTolerance(const modelDefinition* def)
{
    if (!def || !(def->defaultExperiment.tolerance > 0.0)) {
        return MODEL_DEFAULT_TOLERANCE;
    }

    return def->defaultExperiment.tolerance;
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
 * written and read in the same order.
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
    copyBlock(position, model->stateDerivatives, sizeOfStates, save);
}

size_t modelDataSize(const modelDefinition* def)
{
    if (!def) {
        return 0;
    }

    return 2 * sizeof(double) + def->sizeOfParams + def->sizeOfInputs + def->sizeOfOutputs +
           2 * def->numberOfStates * sizeof(double);
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
    free(model->states);
    free(model->stateDerivatives);
    free(model->scratch);

    /* Free the model instance itself */
    free(model);
}
