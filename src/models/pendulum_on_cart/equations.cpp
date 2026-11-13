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

#include "model.h"
#include <cmath>

/**
 * Pendulum on a cart (inverted pendulum) — C++ class-based implementation.
 *
 * Nonlinear equations of motion derived from Lagrangian mechanics:
 *
 *   x_ddot     = (F + m*sin(th)*(l*th_dot^2 + g*cos(th))) / (M + m*sin^2(th))
 *   th_ddot    = (-F*cos(th) - m*l*th_dot^2*cos(th)*sin(th)
 *                 - (M+m)*g*sin(th)) / (l*(M + m*sin^2(th)))
 *
 * States:     x (cart position), x_dot (cart velocity),
 *             theta (pendulum angle from vertical), theta_dot (angular velocity)
 * Parameters: M (cart mass), m (pendulum mass), l (pendulum length), g (gravity)
 * Input:      F (horizontal force on cart)
 */
class PendulumOnCart {
public:
    PendulumOnCart(double* states, double* inputs, double* params)
        : x_dot(states[1]),
          theta(states[2]), theta_dot(states[3]),
          F(inputs[0]),
          M(params[0]), m(params[1]), l(params[2]), g(params[3]) {}

    void derivatives(double* dx) const {
        double sinTh = std::sin(theta);
        double cosTh = std::cos(theta);
        double denom = M + m * sinTh * sinTh;

        dx[0] = x_dot;
        dx[1] = (F + m * sinTh * (l * theta_dot * theta_dot + g * cosTh)) / denom;
        dx[2] = theta_dot;
        dx[3] = (-F * cosTh - m * l * theta_dot * theta_dot * cosTh * sinTh
                 - (M + m) * g * sinTh) / (l * denom);
    }

private:
    double x_dot, theta, theta_dot;
    double F;
    double M, m, l, g;
};

extern "C" {

/**
 * Pendulum on cart state equation: dx/dt = f(x, u)
 *
 * Computes the four state derivatives for the inverted pendulum system.
 */
void pendulumOnCartSystem(modelInstance* model) {
    PendulumOnCart p(model->states, model->inputs, model->parameter);
    p.derivatives(model->stateDerivatives);
}

/**
 * Pendulum on cart output equation: y = h(x, u)
 *
 * Outputs:
 *   y[0] = cart position (x)
 *   y[1] = pendulum angle (theta)
 */
void pendulumOnCartOutputs(modelInstance* model) {
    model->outputs[0] = model->states[0];  /* cart position */
    model->outputs[1] = model->states[2];  /* pendulum angle */
}

}  /* extern "C" */
