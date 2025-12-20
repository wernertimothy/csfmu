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
    modelInfo* info;                          ///< Discrete integration time step [s]
    double* parameter;                        ///< Pointer to model parameters
    double* states;                           ///< Pointer to current state values
    double* stateDerivatives;                 ///< Pointer to computed derivatives
    double* inputs;                           ///< Pointer to input values
    double* outputs;                          ///< Pointer to output values
    getStateDerivatives getStateDerivatives;  ///< Function to compute dx/dt
    integrate integrate;                      ///< Function to perform integration step
    getOutputs getOutputs;                    ///< Functino to compute outputs y
};

/* memory management */
modelInstance* newModel(size_t nx, size_t nu, size_t ny, size_t np);
void freeModel(modelInstance* model);

/* First order system */
// TODO: move to specific model implementation
void firstOrderSystem(modelInstance* model);

/* calculate outputs */
// TODO: move to specific model implementation
void firstOrderOutputs(modelInstance* model);
#endif