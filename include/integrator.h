#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "model.h"

/* Euler Forward x+ = x + dt*dt */
void eulerForward(modelInstance* model, double stepSize);

#endif