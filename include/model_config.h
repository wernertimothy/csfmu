#ifndef CONFIG_H
#define CONFIG_H

/* model identifier */
#define GUID "{a1b2c3d4-e5f6-7890-abcd-ef1234567890}"
#define MODELNAME "fmuTemplate"

/* system dimensions */
#define NUMBER_OF_INPUTS  1
#define NUMBER_OF_PARAMS  2
#define NUMBER_OF_OUTPUTS 1
#define NUMBER_OF_STATES  1

/* initial state */
const double INITIAL_STATE[NUMBER_OF_STATES] = {0.0};

/* initial parameter */
const double INITIAL_PARAMS[NUMBER_OF_PARAMS] = {0.15, 1.0};

/* fmu method for numerical integration */
#define INTEGRATOR_METHOD eulerForward

/* name of the model state equation */
#define MODEL_STATE_EQUATION firstOrderSystem

/* supported fmi logger categories */
#define LOG_ALL       "logAll"
#define LOG_ERROR     "logStatusError"  
#define LOG_WARNING   "logStatusWarning"
#define LOG_EVENT     "logEvent"

#endif