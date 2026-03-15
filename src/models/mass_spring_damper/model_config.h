#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H


/* state equation */
void massSpringDamper(modelInstance* model);
/* output equation */
void massSpringDamperOutputs(modelInstance* model);

/* model identifier */
#define GUID "{b2c3d4e5-f6a7-8901-bcde-f23456789012}"
#define MODELNAME "mass_spring_damper"

/* system dimensions */
#define NUMBER_OF_PARAMS  3
#define NUMBER_OF_INPUTS  1
#define NUMBER_OF_OUTPUTS 2
#define NUMBER_OF_STATES  2

/* initial state */
static const double INITIAL_STATE[NUMBER_OF_STATES] = {0.0, 0.0};

/* initial parameter */
static const double INITIAL_PARAMS[NUMBER_OF_PARAMS] = {
    1.0, /* mass */
    1.0, /* spring stiffness*/
    1.0  /* damping coefficient */
};

/* fmu method for numerical integration */
#define INTEGRATOR_METHOD rungeKutta4
#define MAX_SOLVER_STEPSIZE 1e-3

/* name of the model state equation */
#define MODEL_STATE_EQUATION massSpringDamper

/* name of the model output equation */
#define MODEL_OUTPUT_EQUATION massSpringDamperOutputs

/* supported fmi logger categories */
#define LOG_INFO      "INFO"
#define LOG_ERROR     "ERROR"  
#define LOG_WARNING   "WARNING"

#endif