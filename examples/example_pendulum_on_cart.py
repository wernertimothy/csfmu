"""
Example: Simulating the pendulum_on_cart FMU with fmpy

Demonstrates how to load and simulate the inverted pendulum on a cart
using fmpy, plot the time-series response, and show a 2D animation.

System equations (Lagrangian mechanics):
    x_ddot     = (F + m*sin(th)*(l*th_dot^2 + g*cos(th))) / (M + m*sin^2(th))
    th_ddot    = (-F*cos(th) - m*l*th_dot^2*cos(th)*sin(th)
                  - (M+m)*g*sin(th)) / (l*(M + m*sin^2(th)))

States: x (cart position), x_dot (cart velocity),
        theta (pendulum angle from vertical), theta_dot (angular velocity)

Default parameters: M = 1.0 kg, m = 0.1 kg, l = 0.5 m, g = 9.81 m/s^2
Default initial state: x = 0, x_dot = 0, theta = 0.1 rad, theta_dot = 0

Usage:
    python examples/example_pendulum_on_cart.py
"""

import matplotlib.pyplot as plt
import numpy as np
from fmpy import read_model_description, simulate_fmu
from matplotlib import animation, patches

# Path to the FMU (built with: cmake -DMODEL=pendulum_on_cart .. && cmake --build .)
fmu_path = "build/pendulum_on_cart.fmu"

# Model parameters
M = 1.0    # cart mass [kg]
m = 0.1    # pendulum mass [kg]
l = 0.5    # pendulum length [m]
g = 9.81   # gravity [m/s^2]

# Read model description to inspect variables
model_description = read_model_description(fmu_path)
print("Model: ", model_description.modelName)
print("Variables:")
for var in model_description.modelVariables:
    print(f"  {var.name:>8s}  (vr={var.valueReference}, causality={var.causality})")

# Define input: short force impulse (5 N for 0.5 s, then 0)
input_signal = np.array(
    [(0.0, 5.0), (0.5, 5.0), (0.5, 0.0), (10.0, 0.0)],
    dtype=[("time", np.float64), ("F", np.float64)],
)

# Simulate
result = simulate_fmu(
    fmu_path,
    start_values={"M": M, "m": m, "l": l, "g": g},
    input=input_signal,
    output=["x", "theta"],
    output_interval=0.01,
)

# Extract results
time = result["time"]
x = result["x"]
theta = result["theta"]

# ── Time-series plot ──────────────────────────────────────────────────────────

fig_ts, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 5), sharex=True)

ax1.plot(time, x, linewidth=2)
ax1.set_ylabel("Cart position x [m]")
ax1.set_title(f"Pendulum on Cart (M={M}, m={m}, l={l})")
ax1.grid(True, alpha=0.3)

ax2.plot(time, np.degrees(theta), linewidth=2, color="tab:orange")
ax2.set_xlabel("Time [s]")
ax2.set_ylabel("Pendulum angle [deg]")
ax2.grid(True, alpha=0.3)

fig_ts.tight_layout()

# ── 2D Animation ─────────────────────────────────────────────────────────────

# Cart dimensions (visual only)
cart_w = 0.3
cart_h = 0.15

# Compute pendulum bob positions
bob_x = x + l * np.sin(theta)
bob_y = -l * np.cos(theta)

# Figure setup
fig_anim, ax_anim = plt.subplots(figsize=(10, 4))
ax_anim.set_aspect("equal")
ax_anim.set_xlim(
    min(x.min(), bob_x.min()) - 0.5,
    max(x.max(), bob_x.max()) + 0.5,
)
ax_anim.set_ylim(-l - 0.3, cart_h + 0.3)
ax_anim.set_xlabel("x [m]")
ax_anim.set_title("Pendulum on Cart — Animation")

# Rail
ax_anim.axhline(y=0, color="gray", linewidth=1, linestyle="--")

# Drawing elements
cart_patch = patches.Rectangle(
    (0, 0), cart_w, cart_h,
    facecolor="tab:blue", edgecolor="black", linewidth=1.5,
)
ax_anim.add_patch(cart_patch)
(pendulum_line,) = ax_anim.plot([], [], "k-", linewidth=2.5)
(bob_dot,) = ax_anim.plot([], [], "o", color="tab:red", markersize=10, markeredgecolor="black")
time_text = ax_anim.text(0.02, 0.95, "", transform=ax_anim.transAxes, fontsize=10)

# Subsample for smooth animation (~50 fps display)
step = max(1, len(time) // 500)
anim_indices = range(0, len(time), step)


def init():
    cart_patch.set_xy((-cart_w / 2, 0))
    pendulum_line.set_data([], [])
    bob_dot.set_data([], [])
    time_text.set_text("")
    return cart_patch, pendulum_line, bob_dot, time_text


def update(frame):
    i = anim_indices[frame]
    cx = x[i]

    # Cart
    cart_patch.set_xy((cx - cart_w / 2, 0))

    # Pendulum rod
    pendulum_line.set_data([cx, bob_x[i]], [cart_h / 2, bob_y[i]])

    # Bob
    bob_dot.set_data([bob_x[i]], [bob_y[i]])

    # Time label
    time_text.set_text(f"t = {time[i]:.2f} s")

    return cart_patch, pendulum_line, bob_dot, time_text


anim = animation.FuncAnimation(
    fig_anim,
    update,
    init_func=init,
    frames=len(anim_indices),
    interval=20,
    blit=True,
)

plt.show()
