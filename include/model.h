/**
 * @file model.h
 * @brief Core model infrastructure for FMU template.
 *
 * This module provides the model instance structure, the model definition
 * (the description a model publishes about itself) and memory management.
 * It knows neither FMI nor any specific model:
 *
 * - A model describes itself once in a constant ::modelDefinition: its
 *   variables (name, type, role, start value, ...), its equations and its
 *   integrator.
 * - Generic code (the FMI wrapper, the modelDescription.xml generator, a
 *   standalone simulation) works purely on that description.
 *
 * The design enables:
 * - Model-independent FMI wrapper implementation
 * - Standalone simulation without FMI overhead
 * - Model selection at link time (one MODEL_DEFINITION per binary)
 * - Variables of type double, int and bool, declared in any order
 */

#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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
typedef void (*stateDerivativesFn)(modelInstance* model);

/**
 * @brief Function pointer type for numerical integration.
 *
 * Advances the model state by one time step using a numerical integration
 * method (e.g., Euler forward, Runge-Kutta).
 *
 * @param model Pointer to the model instance to integrate
 * @param stepSize Time step for integration (dt)
 */
typedef void (*integrateFn)(modelInstance* model, double stepSize);

/**
 * @brief Function pointer type for computing model outputs.
 *
 * Model-specific implementations compute y = h(t, x, u) and store
 * the outputs in model->outputs.
 *
 * @param model Pointer to the model instance containing current states and inputs
 */
typedef void (*outputsFn)(modelInstance* model);

/**
 * @brief Data type of a model variable.
 *
 * Corresponds to the C type of the struct field the variable lives in.
 */
typedef enum {
    VAR_REAL,     /**< double */
    VAR_INTEGER,  /**< int */
    VAR_BOOLEAN   /**< bool */
} varType;

/**
 * @brief Role of a model variable.
 *
 * The role also selects the storage block of the model instance the
 * variable lives in (params, inputs, outputs or states).
 */
typedef enum {
    VAR_PARAMETER,  /**< Constant during a simulation run, lives in model->params */
    VAR_INPUT,      /**< Set from outside, lives in model->inputs */
    VAR_OUTPUT,     /**< Computed by the output equation, lives in model->outputs */
    VAR_STATE       /**< Continuous state, lives in model->states */
} varRole;

/**
 * @brief Variability of a parameter.
 */
typedef enum {
    VAR_TUNABLE,  /**< May be changed between simulation steps (default) */
    VAR_FIXED     /**< May only be changed before the simulation starts */
} varVariability;

/**
 * @brief Optional real value (used for bounds that may be absent).
 *
 * A zero-initialized optReal is "not set", so bounds can simply be
 * omitted from a variable declaration.
 */
typedef struct {
    bool   set;    /**< true if value is meaningful */
    double value;  /**< The value, if set */
} optReal;

/**
 * @brief Description of one model variable.
 *
 * Declared with the PARAM(), INPUT(), OUTPUT() and STATE() macros, which
 * fill in name, type, role and offset from the struct field itself.
 */
typedef struct {
    const char*    name;         /**< Published name, unique within the model */
    varType        type;         /**< Data type of the struct field */
    varRole        role;         /**< Role, selects the storage block */
    size_t         offset;       /**< Byte offset of the field inside its storage block */
    double         start;        /**< Start value, converted to the variable's type */
    const char*    unit;         /**< Unit name (optional, real variables only) */
    const char*    description;  /**< Human readable description (optional) */
    optReal        min;          /**< Lower bound (optional) */
    optReal        max;          /**< Upper bound (optional) */
    double         nominal;      /**< Nominal value, 0 = not set (real variables only) */
    varVariability variability;  /**< Parameters only, defaults to VAR_TUNABLE */
} modelVariable;

/**
 * @brief Definition of a unit in terms of SI base-unit exponents.
 *
 * Example: N/m = kg * s^-2  ->  { .name = "N/m", .kg = 1, .s = -2 }
 */
typedef struct {
    const char* name;  /**< Unit name as used in modelVariable.unit */
    int kg, m, s, A, K, mol, cd, rad;  /**< Exponents of the SI base units */
} modelUnit;

/**
 * @brief Suggested simulation settings of a model.
 */
typedef struct {
    double startTime;  /**< Suggested start time */
    double stopTime;   /**< Suggested stop time */
    double tolerance;  /**< Suggested relative tolerance */
    double stepSize;   /**< Suggested (communication) step size */
} modelExperiment;

/**
 * @brief Everything a model publishes about itself.
 *
 * Each model defines exactly one constant of this type, named
 * MODEL_DEFINITION. It is the single source of truth for the model's
 * variables, equations and integration settings.
 */
typedef struct {
    const char* name;         /**< Model name */
    const char* guid;         /**< Globally unique identifier of this model version */
    const char* description;  /**< Human readable description (optional) */
    const char* version;      /**< Model version (optional) */

    const modelVariable* variables;  /**< Table of all published variables */
    size_t numberOfVariables;        /**< Number of entries in variables */

    const modelUnit* units;  /**< Table of unit definitions (optional) */
    size_t numberOfUnits;    /**< Number of entries in units */

    size_t sizeOfParams;    /**< sizeof(Params), 0 if the model has no parameters */
    size_t sizeOfInputs;    /**< sizeof(Inputs), 0 if the model has no inputs */
    size_t sizeOfOutputs;   /**< sizeof(Outputs), 0 if the model has no outputs */
    size_t numberOfStates;  /**< Number of continuous states (nx) */

    stateDerivativesFn getStateDerivatives;  /**< dx/dt = f(t,x,u) */
    outputsFn          getOutputs;           /**< y = h(t,x,u) */
    integrateFn        integrate;            /**< Numerical integration method */
    double             maxStepSize;          /**< Largest allowed integration step */

    modelExperiment defaultExperiment;  /**< Suggested simulation settings */
} modelDefinition;

/**
 * @brief Complete model instance structure.
 *
 * Represents a single instance of a dynamical system model, containing:
 * - A pointer to the model definition
 * - Storage blocks for parameters, inputs and outputs (model-specific structs)
 * - State and state derivative vectors
 * - Function pointers for model equations and integration
 *
 * Memory layout is managed by newModel() and freeModel().
 *
 * The states are stored as a contiguous vector of doubles because the
 * integrators treat them as such. A model may view the vector through a
 * struct consisting only of doubles (see STATE()).
 */
struct modelInstance {
    const modelDefinition* def;              /**< Definition this instance was created from */
    void* params;                            /**< Parameter block [def->sizeOfParams bytes] */
    void* inputs;                            /**< Input block [def->sizeOfInputs bytes] */
    void* outputs;                           /**< Output block [def->sizeOfOutputs bytes] */
    double* states;                          /**< Array of current state values [nx] */
    double* stateDerivatives;                /**< Array of computed state derivatives [nx] */
    stateDerivativesFn getStateDerivatives;  /**< Function to compute dx/dt = f(t,x,u) */
    integrateFn integrate;                   /**< Function to perform time integration */
    outputsFn getOutputs;                    /**< Function to compute y = h(t,x,u) */
};

/**
 * @brief Creates and initializes a new model instance.
 *
 * Allocates memory for a model instance and all its storage blocks as
 * described by the definition, then calls resetModel() so that every
 * variable holds its start value and the function pointers are bound.
 *
 * On allocation failure, any partially allocated memory is freed before
 * returning NULL to prevent memory leaks.
 *
 * @param def Definition of the model to instantiate (must outlive the instance)
 *
 * @return Pointer to newly allocated model instance, or NULL if def is NULL
 *         or allocation fails
 *
 * @note The caller is responsible for freeing the returned instance using freeModel()
 *
 * @see freeModel(), resetModel()
 */
modelInstance* newModel(const modelDefinition* def);

/**
 * @brief Brings a model instance back to its state right after newModel().
 *
 * Zeroes all storage blocks, writes the start value of every variable in
 * the definition's variable table and (re-)binds the function pointers
 * from the definition. This function is NULL-safe.
 *
 * @param model Pointer to the model instance to reset. May be NULL.
 */
void resetModel(modelInstance* model);

/**
 * @brief Returns the address of a variable's storage.
 *
 * The returned pointer must be cast according to the variable's type
 * (VAR_REAL: double*, VAR_INTEGER: int*, VAR_BOOLEAN: bool*).
 *
 * @param model Pointer to the model instance
 * @param index Index into model->def->variables
 *
 * @return Pointer to the variable's storage, or NULL if model is NULL or
 *         index is out of range
 */
void* modelVariableAddress(modelInstance* model, size_t index);

/**
 * @brief Returns the number of bytes needed to store the data of a model.
 *
 * The data of a model are its parameter, input and output blocks, its
 * states and its state derivatives.
 *
 * @param def Definition of the model
 *
 * @return Size in bytes of the buffer used by saveModelData() and
 *         restoreModelData(), or 0 if def is NULL
 */
size_t modelDataSize(const modelDefinition* def);

/**
 * @brief Copies the data of a model instance into a buffer.
 *
 * Together with restoreModelData() this allows taking a snapshot of a model
 * and returning to it later. The buffer is a plain copy of the storage
 * blocks, so it is only valid for the same model definition on the same
 * platform.
 *
 * @param model  Pointer to the model instance
 * @param buffer Buffer of at least modelDataSize(model->def) bytes
 *
 * @see restoreModelData(), modelDataSize()
 */
void saveModelData(const modelInstance* model, void* buffer);

/**
 * @brief Copies data saved with saveModelData() back into a model instance.
 *
 * @param model  Pointer to the model instance
 * @param buffer Buffer filled by saveModelData() for the same model definition
 *
 * @see saveModelData(), modelDataSize()
 */
void restoreModelData(modelInstance* model, const void* buffer);

/**
 * @brief Deallocates a model instance and all its associated memory.
 *
 * Safely frees all dynamically allocated memory within the model instance
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
 * @brief Definition of the model selected for this binary.
 *
 * Every model defines this constant in src/models/<model_name>/variables.c.
 * The model is selected at link time: whichever model is linked into the
 * binary provides the symbol (CMake option -DMODEL=<model_name>).
 */
extern const modelDefinition MODEL_DEFINITION;

#ifdef __cplusplus
}
#endif

/**
 * @brief Macros for declaring model variables.
 *
 * A model declares its storage as plain structs named Params, Inputs,
 * Outputs and States, and lists the fields it wants to publish:
 *
 * @code
 * typedef struct { double tau; int mode; } Params;
 *
 * static const modelVariable VARIABLES[] = {
 *     { PARAM(tau),  .start = 0.15, .unit = "s", .min = SOME(0.001) },
 *     { PARAM(mode), .start = 1 },
 * };
 * @endcode
 *
 * Each macro derives name, role, offset and type from the struct field
 * itself, so these can never disagree with the struct. A field whose type
 * is not double, int or bool (or not double for a state) fails to compile.
 *
 * The macros rely on C11 designated initializers and _Generic and are
 * therefore only available in C. Variable tables always live in a .c file,
 * also for models whose equations are written in C++.
 */
#ifndef __cplusplus

#define MODEL_VARIABLE(roleValue, structType, field, publishedName, ...) \
    .name   = (publishedName), \
    .role   = (roleValue), \
    .offset = offsetof(structType, field), \
    .type   = _Generic(((structType*)0)->field, __VA_ARGS__)

#define MODEL_VARIABLE_TYPES double: VAR_REAL, int: VAR_INTEGER, bool: VAR_BOOLEAN

/** Declares a field of the struct Params as a parameter. */
#define PARAM(field)           MODEL_VARIABLE(VAR_PARAMETER, Params, field, #field, MODEL_VARIABLE_TYPES)

/** Declares a field of the struct Inputs as an input. */
#define INPUT(field)           MODEL_VARIABLE(VAR_INPUT, Inputs, field, #field, MODEL_VARIABLE_TYPES)

/** Declares a field of the struct Outputs as an output. */
#define OUTPUT(field)          MODEL_VARIABLE(VAR_OUTPUT, Outputs, field, #field, MODEL_VARIABLE_TYPES)

/** Declares a field of the struct States as a continuous state (double only). */
#define STATE(field)           MODEL_VARIABLE(VAR_STATE, States, field, #field, double: VAR_REAL)

/**
 * Same as above, but published under a name different from the field name.
 * Needed when two structs use the same field name (e.g. States.x and
 * Outputs.x), since published names must be unique within a model.
 */
#define PARAM_AS(field, name)  MODEL_VARIABLE(VAR_PARAMETER, Params, field, name, MODEL_VARIABLE_TYPES)
#define INPUT_AS(field, name)  MODEL_VARIABLE(VAR_INPUT, Inputs, field, name, MODEL_VARIABLE_TYPES)
#define OUTPUT_AS(field, name) MODEL_VARIABLE(VAR_OUTPUT, Outputs, field, name, MODEL_VARIABLE_TYPES)
#define STATE_AS(field, name)  MODEL_VARIABLE(VAR_STATE, States, field, name, double: VAR_REAL)

/** Sets an optional bound: .min = SOME(0.0) */
#define SOME(realValue) { .set = true, .value = (realValue) }

/** Number of elements of an array (not a pointer). */
#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

/** Number of states in a struct consisting only of doubles. */
#define NUMBER_OF_DOUBLES(structType) (sizeof(structType) / sizeof(double))

#endif /* !__cplusplus */

#endif
