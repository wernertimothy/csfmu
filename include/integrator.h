/**************************************************************************************************
*                                                                                                 *
* This file is part of fmuTemplate.                                                               *
*                                                                                                 *
* fmuTemplate -- A template for Co-Simulation FMUs written in C.                                  *
* Copyright (C) 2025 Timothy Werner                                                               *
*                                                                                                 *
* SPDX-License-Identifier: MIT                                                                    *
*                                                                                                 *
**************************************************************************************************/

#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "model.h"

/* Euler Forward x+ = x + dt*dt */
void eulerForward(modelInstance* model, double stepSize);

#endif