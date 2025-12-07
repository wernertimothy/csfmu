#ifndef MODEL_H
#define MODEL_H

/* Forward declaration of the model instance */
typedef struct modelInstance modelInstance;

/* opaque model functions */
typedef void (*getStateDerivatives)(modelInstance*);
typedef void (*integrate)(modelInstance*, double);

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
    modelInfo* info;                          ///< Discrete integration time step [s]
    double* parameter;                        ///< Pointer to model parameters
    double* states;                           ///< Pointer to current state values
    double* stateDerivatives;                 ///< Pointer to computed derivatives
    double* inputs;                           ///< Pointer to input values
    double* outputs;                          ///< Pointer to output values
    getStateDerivatives getStateDerivatives;  ///< Function to compute dx/dt
    integrate integrate;                      ///< Function to perform integration step
};

/* memory management */
modelInstance* newModel(size_t nx, size_t nu, size_t ny, size_t np);
void freeModel(modelInstance* model);

/* Euler Forward x+ = x + dt*dt */
void eulerForward(modelInstance* model, double stepSize);

/* First order system */
void firstOrderSystem(modelInstance* model);

/* model time step */
void modelDoTimeStep(modelInstance* model, double stepSize);

#endif