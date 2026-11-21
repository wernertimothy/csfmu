"""
Example: Simulating the first_order FMU with fmpy

Demonstrates how to load and simulate the first-order lag system (PT1)
using fmpy, and plot the step response with matplotlib.

System equation:
    dx/dt = (-x + k * u) / tau
    y = x

Default parameters: tau = 0.15 s, k = 1.0
Input: unit step (u = 1.0)

Usage:
    python examples/example_first_order.py
"""

import matplotlib.pyplot as plt
import numpy as np
from fmpy import read_model_description, simulate_fmu

# Path to the FMU (built with: cmake -DMODEL=first_order .. && cmake --build .)
fmu_path = "build/first_order.fmu"

# Read model description to inspect variables
model_description = read_model_description(fmu_path)
print("Model: ", model_description.modelName)
print("Variables:")
for var in model_description.modelVariables:
    print(f"  {var.name:>4s}  (vr={var.valueReference}, causality={var.causality})")

# Define a step input: u switches from 0 to 1 at t = 0
# fmpy expects a structured numpy array with 'time' as the first column
input_signal = np.array(
    [(0.0, 1.0), (10.0, 1.0)],
    dtype=[("time", np.float64), ("u", np.float64)],
)

# Simulate using the default experiment (t = 0..10s, dt = 0.01s)
result = simulate_fmu(
    fmu_path,
    start_values={"tau": 0.15, "k": 1.0},
    input=input_signal,
    output=["y"],
    output_interval=0.01,
)

# Extract results
time = result["time"]
y = result["y"]

# Analytical solution: y(t) = k * (1 - exp(-t / tau))
tau, k = 0.15, 1.0
y_analytical = k * (1.0 - np.exp(-time / tau))

# Plot
fig, ax = plt.subplots(figsize=(8, 4))
ax.plot(time, y, label="FMU simulation", linewidth=2)
ax.plot(time, y_analytical, "--", label="Analytical solution", linewidth=1.5)
ax.set_xlabel("Time [s]")
ax.set_ylabel("Output y")
ax.set_title(f"First-Order Step Response (tau={tau}, k={k})")
ax.legend()
ax.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()
