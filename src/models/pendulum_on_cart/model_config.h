#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H


/* state equation */
void pendulumOnCartSystem(modelInstance* model);
/* output equation */
void pendulumOnCartOutputs(modelInstance* model);

/* model identifier */
#define GUID "{c3d4e5f6-a7b8-9012-cdef-345678901234}"
#define MODELNAME "pendulum_on_cart"

/* system dimensions */
#define NUMBER_OF_PARAMS  4
#define NUMBER_OF_INPUTS  1
#define NUMBER_OF_OUTPUTS 2
#define NUMBER_OF_STATES  4

/* initial state: [x, x_dot, theta, theta_dot] */
static const double INITIAL_STATE[NUMBER_OF_STATES] = {0.0, 0.0, 0.1, 0.0};

/* initial parameter: [M (cart mass), m (pendulum mass), l (pendulum length), g (gravity)] */
static const double INITIAL_PARAMS[NUMBER_OF_PARAMS] = {1.0, 0.1, 0.5, 9.81};

/* fmu method for numerical integration */
#define INTEGRATOR_METHOD rungeKutta4
#define MAX_SOLVER_STEPSIZE 1e-3

/* name of the model state equation */
#define MODEL_STATE_EQUATION pendulumOnCartSystem

/* name of the model output equation */
#define MODEL_OUTPUT_EQUATION pendulumOnCartOutputs

/* supported fmi logger categories */
#define LOG_INFO      "INFO"
#define LOG_ERROR     "ERROR"
#define LOG_WARNING   "WARNING"

#endif
