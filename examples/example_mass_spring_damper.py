"""
Example: Simulating the mass_spring_damper FMU with fmpy

Demonstrates how to load and simulate the mass-spring-damper system
using fmpy, and plot the step response with matplotlib.

System equation:
    m*x'' + d*x' + k*x = u
    => dx/dt = v
       dv/dt = (-k*x - d*v + u) / m

Default parameters: m = 1.0 kg, k = 1.0 N/m, d = 1.0 N.s/m
Input: unit step force (u = 1.0 N)

Usage:
    python examples/example_mass_spring_damper.py
"""

import matplotlib.pyplot as plt
import numpy as np
from fmpy import read_model_description, simulate_fmu

# Path to the FMU (built with: cmake -DMODEL=mass_spring_damper .. && cmake --build .)
fmu_path = "build/mass_spring_damper.fmu"

# Read model description to inspect variables
model_description = read_model_description(fmu_path)
print("Model: ", model_description.modelName)
print("Variables:")
for var in model_description.modelVariables:
    print(f"  {var.name:>4s}  (vr={var.valueReference}, causality={var.causality})")

# Define a step input: u switches from 0 to 1 N at t = 0
# fmpy expects a structured numpy array with 'time' as the first column
input_signal = np.array(
    [(0.0, 1.0), (10.0, 1.0)],
    dtype=[("time", np.float64), ("u", np.float64)],
)

# Simulate using the default experiment (t = 0..10s, dt = 0.001s)
result = simulate_fmu(
    fmu_path,
    start_values={"m": 1.0, "k": 1.0, "d": 1.0},
    input=input_signal,
    output=["x", "v"],
    output_interval=0.01,
)

# Extract results
time = result["time"]
x = result["x"]
v = result["v"]

# Analytical solution for step response of underdamped mass-spring-damper
# m=1, k=1, d=1 => omega_n=1, zeta=0.5 (underdamped)
m_val, k_val, d_val = 1.0, 1.0, 1.0
omega_n = np.sqrt(k_val / m_val)
zeta = d_val / (2.0 * np.sqrt(k_val * m_val))
omega_d = omega_n * np.sqrt(1.0 - zeta**2)
x_ss = 1.0 / k_val  # steady-state displacement

x_analytical = x_ss * (1.0 - np.exp(-zeta * omega_n * time) * (
    np.cos(omega_d * time) + (zeta / np.sqrt(1.0 - zeta**2)) * np.sin(omega_d * time)
))

v_analytical = x_ss * omega_n / np.sqrt(1.0 - zeta**2) * np.exp(
    -zeta * omega_n * time
) * np.sin(omega_d * time)

# Plot
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 6), sharex=True)

ax1.plot(time, x, label="FMU simulation", linewidth=2)
ax1.plot(time, x_analytical, "--", label="Analytical solution", linewidth=1.5)
ax1.set_ylabel("Position x [m]")
ax1.set_title(f"Mass-Spring-Damper Step Response (m={m_val}, k={k_val}, d={d_val})")
ax1.legend()
ax1.grid(True, alpha=0.3)

ax2.plot(time, v, label="FMU simulation", linewidth=2)
ax2.plot(time, v_analytical, "--", label="Analytical solution", linewidth=1.5)
ax2.set_xlabel("Time [s]")
ax2.set_ylabel("Velocity v [m/s]")
ax2.legend()
ax2.grid(True, alpha=0.3)

fig.tight_layout()
plt.show()
