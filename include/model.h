#ifndef MODEL_H
#define MODEL_H

/**
 * @brief Forward declaration of model instance structure
 */
typedef struct modelInstance modelInstance;

/**
 * @brief Model parameters structure
 * 
 * Contains the physical parameters that define the system behavior.
 */
typedef struct {
    double tau; ///< Time constant [s]
    double k;   ///< System gain [-]
} modelParameter;

/**
 * @brief Model state variables structure
 * 
 * Contains the current state values of the dynamic system.
 */
typedef struct {
    double x1; ///< State variable 1
} modelStates;

/**
 * @brief Model state derivatives structure
 * 
 * Contains the computed time derivatives (dx/dt) of the state variables.
 */
typedef struct {
    double dx1; ///< Time derivative of state x1 [1/s]
} modelStateDerivatives;

/**
 * @brief Model input signals structure
 * 
 * Contains the external input signals driving the system.
 */
typedef struct {
    double u; ///< Input signal
} modelInputs;

/**
 * @brief Function pointer type for computing continuous-time derivatives
 * 
 * This function computes the state derivatives dx/dt based on the current
 * state, parameters, and inputs stored in the model instance.
 * 
 * @param[in,out] model Pointer to model instance. The function reads from
 *                      states, parameters, and inputs, and writes to
 *                      stateDerivatives.
 * 
 * @note The function should not modify states, only stateDerivatives
 */
typedef void (*getContinuousTimeDerivative)(modelInstance*);

/**
 * @brief Function pointer type for numerical integration method
 * 
 * This function advances the system state by one time step using a
 * numerical integration algorithm (e.g., Euler, Runge-Kutta).
 * 
 * @param[in,out] model Pointer to model instance. Updates states based
 *                      on stateDerivatives and sampleTime.
 * 
 * @note Must call getStateDerivatives before updating states
 */
typedef void (*integrate)(modelInstance*);

/**
 * @brief Allocate and initialize a model instance
 * 
 * Allocates memory for the model instance and all its sub-structures
 * (parameters, states, derivatives, inputs). All pointers are initialized
 * but values are not set.
 * 
 * @return Pointer to allocated modelInstance on success, NULL on failure
 * 
 * @warning Must be freed using freeModel() to avoid memory leaks
 * @see freeModel()
 */
modelInstance* mallocModel(void);

/**
 * @brief Free a model instance and all associated memory
 * 
 * Deallocates the model instance and all dynamically allocated sub-structures.
 * Safe to call with NULL pointer.
 * 
 * @param[in] model Pointer to model instance to free (may be NULL)
 * 
 * @note After calling this function, the pointer is invalid and should not be used
 */
void freeModel(modelInstance* model);

/**
 * @brief Model instance structure
 * 
 * Complete structure containing all data necessary for dynamic system simulation,
 * including state variables, parameters, derivatives, inputs, sample time, and
 * function pointers for derivative computation and numerical integration.
 */
struct modelInstance {
    double sampleTime;                          ///< Discrete integration time step [s]
    modelParameter* parameter;                  ///< Pointer to model parameters
    modelStates* states;                        ///< Pointer to current state values
    modelStateDerivatives* stateDerivatives;    ///< Pointer to computed derivatives
    modelInputs* inputs;                        ///< Pointer to input signals
    getContinuousTimeDerivative getStateDerivatives; ///< Function to compute dx/dt
    integrate integrate;                        ///< Function to perform integration step
};

/**
 * @brief Euler forward integration method
 * 
 * Implements the explicit Euler (forward Euler) numerical integration:
 * x(k+1) = x(k) + dx/dt * dt
 * 
 * This is a first-order method suitable for systems with small time steps
 * and moderate stability requirements.
 * 
 * @param[in,out] model Pointer to model instance. Calls getStateDerivatives
 *                      and updates states by one time step.
 * 
 * @pre model != NULL
 * @pre model->getStateDerivatives != NULL
 * @pre model->sampleTime > 0
 * 
 * @note This method can be numerically unstable for stiff systems
 */
void eulerForward(modelInstance* model);

/**
 * @brief First-order system derivative function
 * 
 * Computes the state derivative for a first-order linear system:
 * dx/dt = (-x + k*u) / tau
 * 
 * where:
 * - x is the state variable
 * - u is the input signal
 * - k is the system gain
 * - tau is the time constant
 * 
 * @param[in,out] model Pointer to model instance. Reads from states, 
 *                      parameters, and inputs; writes to stateDerivatives.
 * 
 * @pre model != NULL
 * @pre model->parameter->tau != 0
 */
void firstOrderSystem(modelInstance* model);

#endif