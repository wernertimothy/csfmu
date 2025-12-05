#ifndef MODEL_H
#define MODEL_H

/**
 * Forward declaration of the model instance
 */
typedef struct modelInstance modelInstance;

/**
 * Forward declaration of the model parameter
 */
typedef struct modelParameter modelParameter;

/**
 * Meta data of the model
 */
typedef struct {
    // size and dimension
    size_t numberOfStates;    ///> state dimension
    size_t numberOfInputs;    ///> input dimension
    size_t numberOfParameter; ///> parameter dimension
} modelInfo;


typedef void (*getStateDerivatives)(modelInstance*);
typedef void (*integrate)(modelInstance*, double);

// === memory management ===
modelInstance* newModel(size_t nx, size_t nu, size_t np);
void freeModel(modelInstance* model);

/**
 * An instance of a model
 */
struct modelInstance {
    modelInfo* info;                          ///< Discrete integration time step [s]
    double* parameter;                        ///< Pointer to model parameters
    double* states;                           ///< Pointer to current state values
    double* stateDerivatives;                 ///< Pointer to computed derivatives
    double* inputs;                           ///< Pointer to input signals
    getStateDerivatives getStateDerivatives;  ///< Function to compute dx/dt
    integrate integrate;                      ///< Function to perform integration step
};


// === integrators ===

/**
 * Euler Forward x+ = x + dt*dt
 */
void eulerForward(modelInstance* model, double stepSize);

// === model implementations ===

/**
 * First order system
 */
void firstOrderSystem(modelInstance* model);

#endif