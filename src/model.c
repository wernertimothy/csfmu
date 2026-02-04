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

#include <stdlib.h>
#include "model.h"

/**
 * Implementation notes for newModel():
 * - Uses malloc() for modelInstance and modelInfo to avoid zero-initialization overhead
 * - Uses calloc() for all arrays to ensure zero-initialization of data
 * - On any allocation failure, calls freeModel() to clean up partial allocations
 * - Function pointers initialized to NULL; caller must set them before use
 * - Memory allocation order: instance -> info -> parameters -> inputs -> outputs -> states -> derivatives
 */
modelInstance* newModel(size_t nx, size_t nu, size_t ny, size_t np)
{
    modelInstance* model = malloc(sizeof(modelInstance));
    if (!model) {
        return NULL;
    }

    /* Allocate model metadata */
    model->info = malloc(sizeof(modelInfo));
    if (!model->info) {
        free(model);
        return NULL;
    }

    /* Initialize metadata with system dimensions */
    model->info->numberOfParameter = np;
    model->info->numberOfInputs = nu;
    model->info->numberOfOutputs = ny;
    model->info->numberOfStates = nx;

    /* Allocate and zero-initialize parameter array */
    model->parameter = calloc(model->info->numberOfParameter, sizeof(double));
    if (!model->parameter) {
        freeModel(model);
        return NULL;
    }

    /* Allocate and zero-initialize input array */
    model->inputs = calloc(model->info->numberOfInputs, sizeof(double));
    if (!model->inputs) {
        freeModel(model);
        return NULL;
    }

    /* Allocate and zero-initialize output array */
    model->outputs = calloc(model->info->numberOfOutputs, sizeof(double));
    if (!model->outputs) {
        freeModel(model);
        return NULL;
    }

    /* Allocate and zero-initialize state array */
    model->states = calloc(model->info->numberOfStates, sizeof(double));
    if (!model->states) {
        freeModel(model);
        return NULL;
    }

    /* Allocate and zero-initialize state derivative array */
    model->stateDerivatives = calloc(model->info->numberOfStates, sizeof(double));
    if (!model->stateDerivatives) {
        freeModel(model);
        return NULL;
    }

    /* Initialize function pointers to NULL - caller must set these */
    model->getStateDerivatives = NULL;
    model->getOutputs = NULL;
    model->integrate = NULL;

    return model;
}

/**
 * Implementation notes for freeModel():
 * - NULL checks before free() are redundant (C standard guarantees free(NULL) is safe)
 *   but included here for defensive programming and clarity
 * - Order of freeing doesn't matter as there are no pointer dependencies
 * - Always frees the model instance itself last
 */
void freeModel(modelInstance* model)
{
    if (!model) {
        return;
    }

    /* Free all dynamically allocated members */
    free(model->info);
    free(model->parameter);
    free(model->inputs);
    free(model->outputs);
    free(model->states);
    free(model->stateDerivatives);

    /* Free the model instance itself */
    free(model);
}
