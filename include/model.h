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

#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>

/* Forward declaration of the model instance */
typedef struct modelInstance modelInstance;

/* opaque model functions */
typedef void (*getStateDerivatives)(modelInstance*);
typedef void (*integrate)(modelInstance*, double);
typedef void (*getOutputs)(modelInstance*);

/* model meta data */
typedef struct {
    // size and dimension
    size_t numberOfStates;    ///> state dimension
    size_t numberOfInputs;    ///> input dimension
    size_t numberOfOutputs;   ///> output dimension
    size_t numberOfParameter; ///> parameter dimension
} modelInfo;

/* An instance of a model */
/* TODO: typedef Model */
struct modelInstance {
    modelInfo* info;                          ///< Pointer to model meta data
    double* parameter;                        ///< Pointer to model parameters
    double* states;                           ///< Pointer to current state values
    double* stateDerivatives;                 ///< Pointer to computed derivatives
    double* inputs;                           ///< Pointer to input values
    double* outputs;                          ///< Pointer to output values
    getStateDerivatives getStateDerivatives;  ///< Function to compute dx/dt
    integrate integrate;                      ///< Function to perform integration step
    getOutputs getOutputs;                    ///< Function to compute outputs y
};

/* memory management */
modelInstance* newModel(size_t nx, size_t nu, size_t ny, size_t np);
void freeModel(modelInstance* model);

/**
 * build option dependend model configuration
 * 
 * pass model parameter, e.g.: -DMODEL=first_order
 * gracefully defaults to first_order if none is specified
 */

#if defined(MODEL_FIRST_ORDER) 
    #include "first_order/model_config.h"
#else
    #error "No model selected. Use -DMODEL=..."
#endif

#endif