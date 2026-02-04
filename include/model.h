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

/**
 * @file model.h
 * @brief Core model infrastructure for FMU template.
 *
 * This module provides the FMU-agnostic model instance structure and memory
 * management functions. The model representation separates the generic
 * infrastructure (this file) from specific model equations (provided via
 * compile-time configuration in model_config.h).
 *
 * The design enables:
 * - Model-independent FMI wrapper implementation
 * - Standalone simulation without FMI overhead
 * - Compile-time model selection via CMake
 * - Function pointer-based polymorphism for equations and integration
 */

#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>

/**
 * @brief Forward declaration of the model instance structure.
 *
 * Allows function pointer types to reference modelInstance before
 * the complete structure definition.
 */
typedef struct modelInstance modelInstance;

/**
 * @brief Function pointer type for computing state derivatives.
 *
 * Model-specific implementations compute dx/dt = f(t, x, u) and store
 * the derivatives in model->stateDerivatives.
 *
 * @param model Pointer to the model instance containing current states and inputs
 */
typedef void (*getStateDerivatives)(modelInstance* model);

/**
 * @brief Function pointer type for numerical integration.
 *
 * Advances the model state by one time step using a numerical integration
 * method (e.g., Euler forward, Runge-Kutta).
 *
 * @param model Pointer to the model instance to integrate
 * @param stepSize Time step for integration (dt)
 */
typedef void (*integrate)(modelInstance* model, double stepSize);

/**
 * @brief Function pointer type for computing model outputs.
 *
 * Model-specific implementations compute y = h(t, x, u) and store
 * the outputs in model->outputs.
 *
 * @param model Pointer to the model instance containing current states and inputs
 */
typedef void (*getOutputs)(modelInstance* model);

/**
 * @brief Model metadata containing system dimensions.
 *
 * Stores the size of all model arrays to enable bounds checking and
 * generic array allocation.
 */
typedef struct {
    size_t numberOfStates;    /**< Number of continuous state variables (nx) */
    size_t numberOfInputs;    /**< Number of input signals (nu) */
    size_t numberOfOutputs;   /**< Number of output signals (ny) */
    size_t numberOfParameter; /**< Number of model parameters (np) */
} modelInfo;

/**
 * @brief Complete model instance structure.
 *
 * Represents a single instance of a dynamical system model, containing:
 * - Metadata about system dimensions
 * - Arrays for states, inputs, outputs, parameters, and derivatives
 * - Function pointers for model equations and integration
 *
 * Memory layout is managed by newModel() and freeModel(). All arrays are
 * zero-initialized on creation.
 */
struct modelInstance {
    modelInfo* info;                          /**< Pointer to model metadata */
    double* parameter;                        /**< Array of model parameters [np] */
    double* states;                           /**< Array of current state values [nx] */
    double* stateDerivatives;                 /**< Array of computed state derivatives [nx] */
    double* inputs;                           /**< Array of input values [nu] */
    double* outputs;                          /**< Array of output values [ny] */
    getStateDerivatives getStateDerivatives;  /**< Function to compute dx/dt = f(t,x,u) */
    integrate integrate;                      /**< Function to perform time integration */
    getOutputs getOutputs;                    /**< Function to compute y = h(t,x,u) */
};

/**
 * @brief Creates and initializes a new model instance.
 *
 * Allocates memory for a model instance and all its internal arrays
 * (parameters, inputs, outputs, states, and state derivatives). All
 * arrays are zero-initialized using calloc().
 *
 * On allocation failure, any partially allocated memory is freed before
 * returning NULL to prevent memory leaks.
 *
 * @param nx Number of state variables (must be > 0)
 * @param nu Number of input signals (may be 0)
 * @param ny Number of output signals (must be > 0)
 * @param np Number of parameters (may be 0)
 *
 * @return Pointer to newly allocated model instance, or NULL if allocation fails
 *
 * @note The caller is responsible for freeing the returned instance using freeModel()
 * @note Function pointers (getStateDerivatives, integrate, getOutputs) must be
 *       initialized by the caller after creation
 *
 * @see freeModel()
 */
modelInstance* newModel(size_t nx, size_t nu, size_t ny, size_t np);

/**
 * @brief Deallocates a model instance and all its associated memory.
 *
 * Safely frees all dynamically allocated memory within the model instance,
 * including metadata, parameter arrays, input/output arrays, state arrays,
 * and the instance itself. This function is NULL-safe and can be called
 * with a NULL pointer without causing errors.
 *
 * @param model Pointer to the model instance to free. May be NULL.
 *
 * @note After calling this function, the model pointer becomes invalid.
 *       Set the pointer to NULL after calling if reuse might be attempted.
 *
 * @see newModel()
 */
void freeModel(modelInstance* model);

/**
 * @brief Compile-time model configuration selection.
 *
 * The specific model equations and configuration are included at compile time
 * based on the MODEL preprocessor definition (e.g., -DMODEL=first_order).
 * This approach enables:
 * - Zero runtime overhead for model selection
 * - Type-safe compile-time binding of equations
 * - Single FMU binary per model (no runtime polymorphism needed)
 *
 * Each model provides a model_config.h header defining:
 * - GUID and model metadata
 * - System dimensions (NUMBER_OF_STATES, etc.)
 * - Initial values (INITIAL_STATE, INITIAL_PARAMS)
 * - Integration settings (INTEGRATOR_METHOD, MAX_SOLVER_STEPSIZE)
 * - Function mappings (MODEL_STATE_EQUATION, MODEL_OUTPUT_EQUATION)
 *
 * @see src/models/<model_name>/model_config.h for specific implementations
 */
#if defined(MODEL_FIRST_ORDER)
    #include "first_order/model_config.h"
#else
    #error "No model selected. Use -DMODEL=<model_name> in CMake configuration"
#endif

#endif