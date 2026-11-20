#include "variables.h"
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
    PendulumOnCart(const States& states, const Inputs& inputs, const Params& params)
        : x_dot(states.x_dot),
          theta(states.theta), theta_dot(states.theta_dot),
          F(inputs.F),
          M(params.M), m(params.m), l(params.l), g(params.g) {}

    void derivatives(States& dx) const {
        double sinTh = std::sin(theta);
        double cosTh = std::cos(theta);
        double denom = M + m * sinTh * sinTh;

        dx.x         = x_dot;
        dx.x_dot     = (F + m * sinTh * (l * theta_dot * theta_dot + g * cosTh)) / denom;
        dx.theta     = theta_dot;
        dx.theta_dot = (-F * cosTh - m * l * theta_dot * theta_dot * cosTh * sinTh
                        - (M + m) * g * sinTh) / (l * denom);
    }

private:
    double x_dot, theta, theta_dot;
    double F;
    double M, m, l, g;
};

extern "C" {

/**
 * Pendulum on cart state equation: dx/dt = f(t, x, u)
 *
 * Computes the four state derivatives for the inverted pendulum system.
 */
void pendulumOnCartSystem(modelInstance* model) {
    /* View the storage blocks through the structs from variables.h.
     * C++ needs explicit casts where C converts void* implicitly. */
    const States& states = *reinterpret_cast<const States*>(model->states);
    const Inputs& inputs = *static_cast<const Inputs*>(model->inputs);
    const Params& params = *static_cast<const Params*>(model->params);
    States&       dx     = *reinterpret_cast<States*>(model->stateDerivatives);

    PendulumOnCart p(states, inputs, params);
    p.derivatives(dx);
}

/**
 * Pendulum on cart output equation: y = h(t, x, u)
 *
 * Outputs:
 *   x     = cart position
 *   theta = pendulum angle
 */
void pendulumOnCartOutputs(modelInstance* model) {
    const States& states  = *reinterpret_cast<const States*>(model->states);
    Outputs&      outputs = *static_cast<Outputs*>(model->outputs);

    outputs.x     = states.x;      /* cart position */
    outputs.theta = states.theta;  /* pendulum angle */
}

}  /* extern "C" */
