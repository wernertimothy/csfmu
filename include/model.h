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
 * - Variables of type double, int and bool, and string parameters
 *   (char[MODEL_STRING_SIZE]), declared in any order
 */

#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

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
 * @brief Number of vectors of length nx an integrator may use as work memory.
 *
 * The work memory is allocated once by newModel() (see modelInstance.scratch),
 * so integrators need neither the stack nor malloc() for their intermediate
 * results.
 */
#define MODEL_SCRATCH_VECTORS 5

/**
 * @brief Relative tolerance used when a model suggests none.
 *
 * Applies if defaultExperiment.tolerance of the model definition is 0 (not
 * set) and the integration tool does not define a tolerance either.
 */
#define MODEL_DEFAULT_TOLERANCE 1e-6

/**
 * @brief Size of the message buffer of a model instance (see modelError()).
 *
 * Large enough for a message that names a file by its full path.
 */
#define MODEL_MESSAGE_SIZE 1024

/**
 * @brief Size of the struct field of a string parameter, including the terminating '\0'.
 *
 * A string parameter is declared as char name[MODEL_STRING_SIZE] and holds
 * at most MODEL_STRING_SIZE - 1 bytes of UTF-8 text. The text lives inside
 * the parameter block, so a snapshot of the model (see saveModelData())
 * stays a plain copy of memory.
 */
#define MODEL_STRING_SIZE 256

/**
 * @brief Result of a model-layer computation that can fail.
 *
 * FMI-agnostic; the FMI adapter maps it to its own status codes.
 */
typedef enum {
    SIM_OK,       /**< Done */
    SIM_DISCARD,  /**< Numerical failure that the caller may recover from (e.g. with a smaller step) */
    SIM_ERROR     /**< The model or its setup is invalid; the reason is in model->message */
} simStatus;

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
 * Contract: on entry model->time is t and model->stateDerivatives holds
 * f(t, x, u). Before every further derivative evaluation the integrator sets
 * model->time to the time of that stage. On return model->time is t + stepSize.
 * simulateTo() (simulation.h) checks this after every step (up to rounding)
 * and fails with SIM_ERROR otherwise.
 *
 * @param model Pointer to the model instance to integrate
 * @param stepSize Time step for integration (dt)
 */
typedef void (*integrateFn)(modelInstance* model, double stepSize);

/**
 * @brief Function pointer type for computing model outputs and locals.
 *
 * Model-specific implementations compute y = h(t, x, u) and store
 * the outputs in model->outputs and the local variables in model->locals.
 * It is called at the communication points (and whenever a variable was
 * set since the last call), never at the stages of the integrator, so the
 * state equation must not read the locals.
 *
 * @param model Pointer to the model instance containing current states and inputs
 */
typedef void (*outputsFn)(modelInstance* model);

/**
 * @brief Function pointer type for computing the calculated parameters.
 *
 * Model-specific implementations compute the calculated parameters (see
 * CALCULATED_PARAM()) from the parameters and store them in model->params.
 * They read nothing but the parameters, because the function is called
 * whenever a parameter may have changed: before a variable is read after a
 * set, before the initialization and at the start of a step after a set.
 *
 * @param model Pointer to the model instance
 */
typedef void (*calculateParametersFn)(modelInstance* model);

/**
 * @brief Function pointer type for the initialization of a model.
 *
 * Called once per simulation when the initialization is finished
 * (fmi2ExitInitializationMode): after all start values and parameters are
 * set and the calculated parameters are computed, before the outputs are
 * computed for the first communication point. It may set the states that
 * are declared with .initial = VAR_APPROX, and the internals.
 *
 * @param model Pointer to the model instance
 *
 * @return SIM_OK, or the result of modelError() if the model cannot start
 */
typedef simStatus (*initializeFn)(modelInstance* model);

/**
 * @brief Data type of a model variable.
 *
 * Corresponds to the C type of the struct field the variable lives in.
 */
typedef enum {
    VAR_REAL,     /**< double */
    VAR_INTEGER,  /**< int */
    VAR_BOOLEAN,  /**< bool */
    VAR_STRING    /**< char[MODEL_STRING_SIZE], parameters only */
} varType;

/**
 * @brief Role of a model variable.
 *
 * The role also selects the storage block of the model instance the
 * variable lives in (params, inputs, outputs, locals or states).
 */
typedef enum {
    VAR_PARAMETER,  /**< Constant during a simulation run, lives in model->params */
    VAR_INPUT,      /**< Set from outside, lives in model->inputs */
    VAR_OUTPUT,     /**< Computed by the output equation, lives in model->outputs */
    VAR_STATE,      /**< Continuous state, lives in model->states */
    VAR_LOCAL,      /**< Computed by the output equation and published read-only, but not
                         part of the interface (no output), lives in model->locals */
    VAR_CALCULATED_PARAMETER  /**< Computed from the parameters by calculateParameters and
                                   published read-only, lives in model->params */
} varRole;

/**
 * @brief Variability of a parameter.
 */
typedef enum {
    VAR_TUNABLE,  /**< May be changed between simulation steps (default) */
    VAR_FIXED     /**< May only be changed before the simulation starts */
} varVariability;

/**
 * @brief How a state is initialized.
 */
typedef enum {
    VAR_EXACT,   /**< The state starts with its start value (default) */
    VAR_APPROX   /**< The start value is a guess: the model's initialize function may
                      replace it (e.g. by a steady state) */
} varInitial;

/**
 * @brief One item of an enumeration type.
 */
typedef struct {
    const char* name;         /**< Published name of the item, unique within its type */
    int         value;        /**< Value of the item, unique within its type */
    const char* description;  /**< Human readable description (optional) */
} modelEnumerationItem;

/**
 * @brief An enumeration type: a named set of integer values.
 *
 * A variable of type int is published as an enumeration by pointing at its
 * type (modelVariable.declaredType). It is still accessed like an Integer
 * variable, but only the values of the items can be set. The field itself
 * stays an int: the underlying type of a C enum is implementation-defined,
 * so its type would not be recognized the same way on every compiler.
 *
 * @code
 * static const modelEnumerationItem INIT_MODE_ITEMS[] = {
 *     { .name = "fixed_start",  .value = 1, .description = "T starts at its start value" },
 *     { .name = "steady_state", .value = 2, .description = "T starts in equilibrium" },
 * };
 * static const modelEnumeration INIT_MODE = {
 *     .name = "InitMode", .items = INIT_MODE_ITEMS, .numberOfItems = ARRAY_LEN(INIT_MODE_ITEMS),
 * };
 * @endcode
 */
typedef struct {
    const char*                 name;           /**< Published name of the type, unique within the model */
    const char*                 description;    /**< Human readable description (optional) */
    const modelEnumerationItem* items;          /**< The items of the type */
    size_t                      numberOfItems;  /**< Number of entries in items */
} modelEnumeration;

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
 * Declared with the PARAM(), CALCULATED_PARAM(), INPUT(), OUTPUT(), LOCAL() and STATE() macros, which
 * fill in name, type, role, offset and size from the struct field itself.
 */
typedef struct {
    const char*             name;         /**< Published name, unique within the model */
    varType                 type;         /**< Data type of the struct field */
    varRole                 role;         /**< Role, selects the storage block */
    size_t                  offset;       /**< Byte offset of the field inside its storage block */
    size_t                  size;         /**< sizeof the struct field, filled in by the macros (the
                                               build checks it for strings: a char* pointer has the
                                               same type for _Generic, but not the same size) */
    double                  start;        /**< Start value, converted to the variable's type
                                               (not for strings, see startString) */
    const char*             startString;  /**< String parameters only: start value, NULL (default) = "" */
    const char*             unit;         /**< Unit name (optional, real variables only) */
    const char*             description;  /**< Human readable description (optional) */
    optReal                 min;          /**< Lower bound (optional) */
    optReal                 max;          /**< Upper bound (optional) */
    double                  nominal;      /**< Nominal value, 0 = not set (real variables only) */
    varVariability          variability;  /**< Parameters and calculated parameters only, defaults to VAR_TUNABLE */
    varInitial              initial;      /**< States only, defaults to VAR_EXACT */
    const modelEnumeration* declaredType; /**< Integer variables only: the enumeration type of the
                                               variable, NULL (default) = plain Integer */
    const char*             dependsOn;    /**< Outputs only: names of the inputs the output depends on
                                               directly, separated by spaces. "" = none,
                                               NULL (default) = all inputs */
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
    size_t sizeOfLocals;    /**< sizeof(Locals), 0 if the model has no local variables */
    size_t sizeOfInternals; /**< sizeof(Internals), 0 if the model has no internals. Internals are
                                 memory of the model that is not published: zeroed by
                                 resetModel() and part of the model data (see saveModelData()) */
    size_t numberOfStates;  /**< Number of continuous states (nx) */

    stateDerivativesFn getStateDerivatives;  /**< dx/dt = f(t,x,u) */
    outputsFn          getOutputs;           /**< y = h(t,x,u) */
    integrateFn        integrate;            /**< Numerical integration method */
    double             maxStepSize;          /**< Largest allowed integration step */

    calculateParametersFn calculateParameters;  /**< Calculated parameters (optional, required
                                                     if the model has calculated parameters) */
    initializeFn          initialize;           /**< Initialization (optional) */

    modelExperiment defaultExperiment;  /**< Suggested simulation settings */
} modelDefinition;

/**
 * @brief Complete model instance structure.
 *
 * Represents a single instance of a dynamical system model, containing:
 * - A pointer to the model definition
 * - The current time t and the relative tolerance
 * - Storage blocks for parameters, inputs, outputs and local variables
 *   (model-specific structs)
 * - A storage block for the internals (model-specific struct, not published)
 * - State and state derivative vectors
 * - The initial guesses of the states, which every initialization starts
 *   from (see beginInitialization())
 * - Work memory for the integrator (content is undefined between calls; it is
 *   neither reset by resetModel() nor part of saveModelData())
 * - A buffer for rolling back a failed step (see stepStart)
 * - Function pointers for model equations and integration
 * - A message buffer that explains the last error (see modelError())
 * - The path of the folder with the model's data files (see resourcePath)
 *
 * Memory layout is managed by newModel() and freeModel().
 *
 * The states are stored as a contiguous vector of doubles because the
 * integrators treat them as such. A model may view the vector through a
 * struct consisting only of doubles (see STATE()).
 */
struct modelInstance {
    const modelDefinition* def;              /**< Definition this instance was created from */
    double time;                             /**< Current time t. simulateTo() sets it at the start of every
                                                  integration step and to the end of the interval; the
                                                  integrator sets the times of its stages in between */
    double relTol;                           /**< Relative tolerance for adaptive integrators (not used by fixed-step methods) */
    void* params;                            /**< Parameter block [def->sizeOfParams bytes] */
    void* inputs;                            /**< Input block [def->sizeOfInputs bytes] */
    void* outputs;                           /**< Output block [def->sizeOfOutputs bytes] */
    void* locals;                            /**< Local variable block [def->sizeOfLocals bytes], computed
                                                  by the output equation together with the outputs */
    void* internals;                         /**< Internals block [def->sizeOfInternals bytes], not published.
                                                  The state and output equations only read it */
    double* states;                          /**< Array of current state values [nx] */
    double* stateDerivatives;                /**< Array of computed state derivatives [nx] */
    double* initialGuess;                    /**< The states as the initialization found them [nx]: every
                                                  initializeModel() after the first sets the states with
                                                  .initial = VAR_APPROX back to these guesses before it
                                                  runs. Part of the model data */
    bool hasInitialGuess;                    /**< true once initializeModel() has taken initialGuess; false
                                                  after newModel(), resetModel() and beginInitialization().
                                                  Part of the model data */
    double* scratch;                         /**< Work memory for the integrator [MODEL_SCRATCH_VECTORS * nx] */
    void* stepStart;                         /**< Model data at the start of the current step, written by
                                                  saveModelData() and used to roll back a failed step
                                                  [modelDataSize(def) bytes]. Not part of the model data */
    stateDerivativesFn getStateDerivatives;  /**< Function to compute dx/dt = f(t,x,u) */
    integrateFn integrate;                   /**< Function to perform time integration */
    outputsFn getOutputs;                    /**< Function to compute y = h(t,x,u) */
    char message[MODEL_MESSAGE_SIZE];        /**< Reason of the last SIM_ERROR or SIM_DISCARD (see modelError(), modelDiscard()), "" if
                                                  none. Cleared by resetModel(), not part of the model data */
    char* resourcePath;                      /**< Absolute path of the folder with the model's data files
                                                  (resources), UTF-8, ending with a '/'; NULL if unknown.
                                                  Set by the caller with modelSetResourcePath(), read-only for
                                                  the model. Kept by resetModel(), not part of the model data */
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
 * Sets the time to 0 and the relative tolerance to the model's default
 * (see modelDefaultTolerance()), zeroes all storage blocks (including the
 * internals, which have no start values), writes the start value of every
 * variable in the definition's variable table, (re-)binds the function
 * pointers from the definition, forgets the initial guesses (see
 * beginInitialization()) and clears the message. The resource path
 * (see modelSetResourcePath()) is kept: it belongs to the instance, not to
 * a simulation. This function is NULL-safe.
 *
 * @param model Pointer to the model instance to reset. May be NULL.
 */
void resetModel(modelInstance* model);

/**
 * @brief Returns the default relative tolerance of a model.
 *
 * @param def Definition of the model
 *
 * @return def->defaultExperiment.tolerance if it is set (> 0), otherwise
 *         MODEL_DEFAULT_TOLERANCE (also if def is NULL)
 */
double modelDefaultTolerance(const modelDefinition* def);

/**
 * @brief Computes the calculated parameters of a model.
 *
 * Calls def->calculateParameters if the model has one, otherwise does
 * nothing. It may be called any number of times.
 *
 * @param model Pointer to the model instance
 */
void computeCalculatedParameters(modelInstance* model);

/**
 * @brief Starts a new initialization of a model.
 *
 * The next initializeModel() takes the states as they are then as the
 * initial guesses, which it and every later call start from. The guesses
 * (the start values of the states with .initial = VAR_APPROX) can therefore
 * still be set until that call. Call it when a new initialization begins
 * (FMI: fmi2EnterInitializationMode). This function is NULL-safe.
 *
 * @param model Pointer to the model instance. May be NULL.
 *
 * @see initializeModel()
 */
void beginInitialization(modelInstance* model);

/**
 * @brief Initializes a model for a simulation.
 *
 * Computes the calculated parameters, then calls def->initialize if the
 * model has one. Call it after the start values and parameters are set and
 * before the outputs are computed for the first time.
 *
 * It may be called more than once, e.g. whenever the caller has changed a
 * parameter or an input during the initialization: every call initializes
 * the model for the current values. The first call after newModel(),
 * resetModel() or beginInitialization() takes the current states as the
 * initial guesses (model->initialGuess). Every later call first sets the
 * states with .initial = VAR_APPROX back to these guesses, so its result does
 * not depend on the calls before; the other states keep the values the
 * caller set. A single call behaves exactly like an initialization without
 * guesses.
 *
 * @param model Pointer to the model instance
 *
 * @return SIM_OK, or the status of the model's initialize function (the
 *         reason is in model->message); SIM_ERROR if model is NULL
 *
 * @see beginInitialization()
 */
simStatus initializeModel(modelInstance* model);

/**
 * @brief Reports why a model cannot continue.
 *
 * Formats the message like printf into model->message (truncated to
 * MODEL_MESSAGE_SIZE) and returns SIM_ERROR, so that a model function can
 * fail with "return modelError(model, ...)".
 *
 * @param model  Pointer to the model instance
 * @param format printf-like format of the message
 *
 * @return SIM_ERROR
 */
simStatus modelError(modelInstance* model, const char* format, ...);

/**
 * @brief Reports a numerical failure that the caller may recover from.
 *
 * Like modelError(), but returns SIM_DISCARD: the computation failed for
 * numerical reasons (e.g. a state became NaN), not because the model or its
 * setup is invalid. The caller restores the model and may try again, e.g.
 * with a smaller step.
 *
 * @param model  Pointer to the model instance
 * @param format printf-like format of the message
 *
 * @return SIM_DISCARD
 */
simStatus modelDiscard(modelInstance* model, const char* format, ...);

/**
 * @brief Returns the published name of a state, for messages.
 *
 * @param def   The model definition
 * @param index Index of the state in the state vector (0 ... nx - 1)
 *
 * @return The name of the state's variable, or "?" if there is none
 */
const char* modelStateName(const modelDefinition* def, size_t index);

/**
 * @brief Finds the first state that is NaN or infinite.
 *
 * @param model Pointer to the model instance
 *
 * @return Index of that state, or numberOfStates if all states are finite
 */
size_t modelFirstNonFiniteState(const modelInstance* model);

/**
 * @brief Finds the first result of the output equation that is NaN or infinite.
 *
 * The results are the Real outputs and the Real local variables, which an
 * importer can read.
 *
 * @param model Pointer to the model instance
 *
 * @return Index of that variable in model->def->variables, or
 *         numberOfVariables if all of them are finite
 */
size_t modelFirstNonFiniteResult(modelInstance* model);

/**
 * @brief Returns what kind of result a variable is, for messages.
 *
 * @param variable An output or a local variable
 *
 * @return "Local variable" or "Output"
 */
const char* modelResultKind(const modelVariable* variable);

/**
 * @brief Checks whether a value is the value of an item of an enumeration.
 *
 * @param type  The enumeration type (may be NULL)
 * @param value The value to check
 *
 * @return true if one of the items of type has the value, false otherwise
 *         (also if type is NULL)
 */
bool modelEnumerationHasValue(const modelEnumeration* type, int value);

/**
 * @brief Sets the value of a string variable.
 *
 * Copies value (including the terminating '\0') into the variable's
 * char[MODEL_STRING_SIZE] field. A value that does not fit is refused, never
 * cut: the variable then keeps its value. value may point into the variable
 * itself (e.g. a pointer the caller read before), the copy handles the overlap.
 *
 * @param model Pointer to the model instance
 * @param index Index into model->def->variables
 * @param value '\0'-terminated text of at most MODEL_STRING_SIZE - 1 bytes
 *
 * @return true if the value was set; false if model or value is NULL, index
 *         is out of range, the variable is not a string of MODEL_STRING_SIZE
 *         bytes or the value does not fit (the variable is unchanged in all
 *         these cases)
 */
bool modelSetString(modelInstance* model, size_t index, const char* value);

/**
 * @brief Returns the address of a variable's storage.
 *
 * The returned pointer must be cast according to the variable's type
 * (VAR_REAL: double*, VAR_INTEGER: int*, VAR_BOOLEAN: bool*, VAR_STRING:
 * char*, pointing at a '\0'-terminated text in a char[MODEL_STRING_SIZE]).
 * Write strings with modelSetString(), which checks the length.
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
 * The data of a model are its time, its relative tolerance, its parameter,
 * input and output blocks, its states, its state derivatives, its internals,
 * its local variables and the initial guesses of its states (see
 * beginInitialization()).
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
 * and returning to it later. The buffer is a plain copy of the time, the
 * relative tolerance and the storage blocks (including the internals and the
 * initial guesses), so it
 * is only valid for the same model definition on the same platform. For the
 * same reason the internals hold plain values and fixed-size arrays only: a
 * pointer would be copied, not the memory it points to.
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
 * @brief Sets the path of the folder with the model's data files.
 *
 * Copies path into model->resourcePath and appends a '/' unless it already
 * ends with one (or with a backslash on Windows), so that a model can append a
 * file name directly. The path belongs to the instance, like its definition:
 * resetModel() keeps it and it is not part of the model data.
 *
 * @param model Pointer to the model instance
 * @param path  Absolute path, UTF-8 encoded; NULL or "" means unknown
 *              (model->resourcePath becomes NULL)
 *
 * @return true on success, false if model is NULL or memory allocation
 *         fails (model->resourcePath is then NULL)
 */
bool modelSetResourcePath(modelInstance* model, const char* path);

/**
 * @brief Opens a file whose path is UTF-8 encoded.
 *
 * Same as fopen(), but the path is always UTF-8, also on Windows, where
 * fopen() would read it in the local code page and fail on non-ASCII
 * characters (e.g. a temporary folder below a user name with an umlaut).
 *
 * @param path UTF-8 encoded path
 * @param mode Mode as for fopen(), e.g. "rb"
 *
 * @return The opened file (close it with fclose()), or NULL if path or mode
 *         is NULL, path is not valid UTF-8 (Windows) or the file cannot be
 *         opened
 */
FILE* modelOpenFile(const char* path, const char* mode);

/**
 * @brief Deallocates a model instance and all its associated memory.
 *
 * Safely frees all dynamically allocated memory within the model instance
 * (including the resource path) and the instance itself. This function is
 * NULL-safe and can be called with a NULL pointer without causing errors.
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
 * Outputs, Locals and States, and lists the fields it wants to publish:
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
 * Each macro derives name, role, offset, size and type from the struct field
 * itself, so these can never disagree with the struct. A field whose type
 * is not double, int or bool (or not double for a state) fails to compile.
 * Parameters may also be strings, declared as char name[MODEL_STRING_SIZE]
 * (the start value goes into .startString); the array is seen as char* by
 * _Generic, and the build checks its size.
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
    .size   = sizeof(((structType*)0)->field), \
    .type   = _Generic(((structType*)0)->field, __VA_ARGS__)

#define MODEL_VARIABLE_TYPES double: VAR_REAL, int: VAR_INTEGER, bool: VAR_BOOLEAN

/* Parameters may also be strings: _Generic sees a char array as char* */
#define MODEL_PARAMETER_TYPES MODEL_VARIABLE_TYPES, char*: VAR_STRING

/** Declares a field of the struct Params as a parameter (also a string: char name[MODEL_STRING_SIZE]). */
#define PARAM(field)           MODEL_VARIABLE(VAR_PARAMETER, Params, field, #field, MODEL_PARAMETER_TYPES)

/** Declares a field of the struct Params as a calculated parameter (computed, published read-only). */
#define CALCULATED_PARAM(field) \
    MODEL_VARIABLE(VAR_CALCULATED_PARAMETER, Params, field, #field, MODEL_VARIABLE_TYPES)

/** Declares a field of the struct Inputs as an input. */
#define INPUT(field)           MODEL_VARIABLE(VAR_INPUT, Inputs, field, #field, MODEL_VARIABLE_TYPES)

/** Declares a field of the struct Outputs as an output. */
#define OUTPUT(field)          MODEL_VARIABLE(VAR_OUTPUT, Outputs, field, #field, MODEL_VARIABLE_TYPES)

/** Declares a field of the struct Locals as a local variable (published, read-only). */
#define LOCAL(field)           MODEL_VARIABLE(VAR_LOCAL, Locals, field, #field, MODEL_VARIABLE_TYPES)

/** Declares a field of the struct States as a continuous state (double only). */
#define STATE(field)           MODEL_VARIABLE(VAR_STATE, States, field, #field, double: VAR_REAL)

/**
 * Same as above, but published under a name different from the field name.
 * Needed when two structs use the same field name (e.g. States.x and
 * Outputs.x), since published names must be unique within a model.
 */
#define PARAM_AS(field, name)  MODEL_VARIABLE(VAR_PARAMETER, Params, field, name, MODEL_PARAMETER_TYPES)
#define INPUT_AS(field, name)  MODEL_VARIABLE(VAR_INPUT, Inputs, field, name, MODEL_VARIABLE_TYPES)
#define OUTPUT_AS(field, name) MODEL_VARIABLE(VAR_OUTPUT, Outputs, field, name, MODEL_VARIABLE_TYPES)
#define LOCAL_AS(field, name)  MODEL_VARIABLE(VAR_LOCAL, Locals, field, name, MODEL_VARIABLE_TYPES)
#define CALCULATED_PARAM_AS(field, name) \
    MODEL_VARIABLE(VAR_CALCULATED_PARAMETER, Params, field, name, MODEL_VARIABLE_TYPES)
#define STATE_AS(field, name)  MODEL_VARIABLE(VAR_STATE, States, field, name, double: VAR_REAL)

/** Sets an optional bound: .min = SOME(0.0) */
#define SOME(realValue) { .set = true, .value = (realValue) }

/** Number of elements of an array (not a pointer). */
#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

/** Number of states in a struct consisting only of doubles. */
#define NUMBER_OF_DOUBLES(structType) (sizeof(structType) / sizeof(double))

#endif /* !__cplusplus */

#endif
