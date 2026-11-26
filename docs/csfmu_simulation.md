# Simulating a model

`csfmu simulate` runs the FMU of a model with a small co-simulation master and writes the trajectory to a CSV file.
You can use it to look at what a model does, and to record a trajectory before a change to the FMU and compare it with the one after, bit for bit.

```bash
uv run csfmu simulate first_order
```

This builds `first_order`, simulates its default experiment and writes `first_order.csv` to the current directory.

The model is **always built first**. The build is incremental, so this costs next to nothing when nothing has changed, and a result never comes from an FMU that is older than its sources.

## Options

  | Option | Default | Meaning |
  |--------|---------|---------|
  | `-i`, `--input <file>` | none | CSV file with the inputs over time (see [Input file](#input-file)) |
  | `-o`, `--output <file>` | `<model>.csv` | CSV file the trajectory is written to, relative to the current directory |
  | `--start <t>` | `startTime` of the default experiment | Start time |
  | `--stop <t>` | `stopTime` of the default experiment | Stop time, a whole number of steps after the start time |
  | `--step <h>` | `stepSize` of the default experiment | Communication step size |
  | `--tolerance <tol>` | the FMU's own | Relative tolerance, passed to `fmi2SetupExperiment` |
  | `--set <name>=<value>` | none | Value of a parameter, input or state, set before the initialization. Repeat it for several variables |

The default experiment is what the model suggests in `.defaultExperiment` of its `MODEL_DEFINITION` (see [getting started](csfmu_gettingStarted.md)).

## What the master does

The master runs the FMU through its FMI 2.0 interface, the same way any other importer would:

1. `fmi2SetupExperiment` with the start time, the stop time and, if given, the tolerance
2. the values of `--set`, then the inputs of the start time
3. `fmi2EnterInitializationMode` and `fmi2ExitInitializationMode`
4. at every communication point **t<sub>k</sub>**: record a row, `fmi2DoStep` to **t<sub>k+1</sub>**, set the inputs of **t<sub>k+1</sub>**
5. `fmi2Terminate` after the last row

The communication points are **t<sub>k</sub> = start + k·step**, computed from the integer k each time, never by adding up the step size.
So the time column shows exactly the times the FMU was given, including rounding: with a step of `0.1`, the fourth row is at `0.30000000000000004`.

A row holds the values after the inputs of its time were set. An output with direct feedthrough therefore already shows the new input in the same row (see `pi_controller`).

The FMU's warnings and errors are printed with its own message. If a step fails, the simulation stops, and the file holds all rows up to the last communication point the FMU reached.
`fault_injection` shows this. With its fault switched on, the step from 0.5 to 0.6 is discarded:

```
$ uv run csfmu simulate fault_injection --set fail=true
...
[DISCARD] State 'x' is not finite at t = 0.55. Step from 0.5 to 0.6 discarded, FMU rolled back to 0.5.
Wrote 6 rows (t = 0.0 to 0.5) to /path/to/fault_injection.csv
Error: the simulation stopped at the step from t = 0.5: fmi2DoStep failed with status 2 (discard).
```

`csfmu simulate` exits with 0 if the simulation reached the stop time, and with 1 otherwise.

## Input file

The input file is a CSV file with a header row. The first column is `time`, and every other column is named after an input of the model.
Inputs that have no column keep their start value.

```
time,u
0.02,1
0.05,0.5
```

The inputs are held constant between the rows (zero-order hold): at a communication point an input takes the value of the last row whose time is not after it.
The rows don't have to be on the communication points, and before the first row an input keeps its start value.
A step needs only one row. If several rows have the same time, the last one counts.

The times must not decrease. Real values are written as numbers (`1`, `0.5`, `1e-3`), Integer and Enumeration values as whole numbers, Boolean values as `true`, `false`, `1` or `0`.

With the file above, `first_order` gets a step at 0.02 and another one at 0.05:

```
$ uv run csfmu simulate first_order -i step.csv --stop 0.07
$ cat first_order.csv
time,u,y,x
0.0,0.0,0.0,0.0
0.01,0.0,0.0,0.0
0.02,1.0,0.0,0.0
0.03,1.0,0.06470181070755977,0.06470181070755977
0.04,1.0,0.12521729710628265,0.12521729710628265
0.05,0.5,0.18181732195915942,0.18181732195915942
0.06,0.5,0.2024043173641823,0.2024043173641823
0.07,0.5,0.22165929688947197,0.22165929688947197
```

## Result file

The result file has a header row with `time` and the names of the recorded variables, then one row per communication point.
It records every variable that can change during a simulation: the inputs, the outputs, the states and the local variables, in the order of the `modelDescription.xml`.
Parameters and calculated parameters stay constant and are not recorded.

Every value is written exactly:

* Real values with the shortest text that reads back to the same double: `0.1` stays `0.1`, and a value that is not quite 0.1 shows all of its digits
* Integer and Enumeration values as whole numbers, Boolean values as `1` and `0`
* `nan`, `inf` and `-inf` as such
* every line ends with `\n`, on every platform

`typed_demo` shows all three types. Its state `x` is a ramp with slope 1, and the file shows the rounding errors the integration adds up along the way:

```
time,limit_reached,enable,x,level,boost,y
0.0,0,1,0.0,0,1,0.0
0.01,0,1,0.010000000000000002,0,1,0.010000000000000002
0.02,0,1,0.02000000000000001,0,1,0.02000000000000001
```

Any CSV reader can load the file. In Python, for example:

```python
import numpy as np

result = np.genfromtxt("first_order.csv", delimiter=",", names=True)
```

## Comparing two builds

Because every value is written exactly, two trajectories are bit-identical exactly when their files are byte-identical.
So you need no special tool to check that a change to the FMU didn't change the results: `cmp` does it.

```bash
uv run csfmu simulate mass_spring_damper -o before.csv
# ... change the integrator, the FMI wrapper, ...
uv run csfmu simulate mass_spring_damper -o after.csv
cmp before.csv after.csv && echo "bit-identical"
```

`cmp` names the first byte that differs. `diff before.csv after.csv` shows the rows.

To compare every model, record them all into a folder before the change and again after it:

```bash
mkdir -p before
for model in $(uv run csfmu list); do uv run csfmu simulate "$model" -o "before/$model.csv"; done
# ... change ...
mkdir -p after
for model in $(uv run csfmu list); do uv run csfmu simulate "$model" -o "after/$model.csv"; done
diff -r before after && echo "all bit-identical"
```

Use the same settings for both runs (`--input`, `--set`, the times), and compare on the same machine with the same build folder.
Mathematical functions such as `exp` or `sin` round differently on Linux, macOS and Windows, so a trajectory recorded on one platform may differ from another platform's in the last bits.
