# FMI 2.0 API Reference

This document provides a comprehensive API reference for FMI 2.0 functions implemented in this template, based on the [FMI Standard](https://fmi-standard.org/).

## Table of Contents

- [Inquiry Functions](#inquiry-functions)
  - [fmi2GetTypesPlatform](#fmi2gettypesplatform)
  - [fmi2GetVersion](#fmi2getversion)
- [Instance Management](#instance-management)
  - [fmi2Instantiate](#fmi2instantiate)
  - [fmi2FreeInstance](#fmi2freeinstance)
  - [fmi2SetupExperiment](#fmi2setupexperiment)
  - [fmi2SetDebugLogging](#fmi2setdebuglogging)
- [Initialization](#initialization)
  - [fmi2EnterInitializationMode](#fmi2enterinitializationmode)
  - [fmi2ExitInitializationMode](#fmi2exitinitializationmode)
- [Variable Access](#variable-access)
  - [fmi2SetReal](#fmi2setreal)
  - [fmi2GetReal](#fmi2getreal)
  - [fmi2SetInteger / fmi2GetInteger](#fmi2setinteger--fmi2getinteger)
  - [fmi2SetBoolean / fmi2GetBoolean](#fmi2setboolean--fmi2getboolean)
  - [fmi2SetString / fmi2GetString](#fmi2setstring--fmi2getstring)
- [Simulation](#simulation)
  - [fmi2DoStep](#fmi2dostep)
  - [fmi2GetStatus / fmi2GetXXXStatus](#fmi2getstatus--fmi2getxxxstatus)
- [FMU State](#fmu-state)
  - [fmi2GetFMUstate](#fmi2getfmustate)
  - [fmi2SetFMUstate](#fmi2setfmustate)
  - [fmi2FreeFMUstate](#fmi2freefmustate)
  - [fmi2SerializedFMUstateSize / fmi2SerializeFMUstate / fmi2DeSerializeFMUstate](#fmi2serializedfmustatesize--fmi2serializefmustate--fmi2deserializefmustate)
- [Shutdown](#shutdown)
  - [fmi2Terminate](#fmi2terminate)
  - [fmi2Reset](#fmi2reset)
- [Unsupported Functions](#unsupported-functions)

## Error Handling

All functions follow the same rules:

- A function that fails returns `fmi2Error` and puts the FMU into the **Error** state. This includes calls that are not allowed in the current state, invalid arguments and unknown value references.
- In the Error state every further call fails, except: `fmi2Reset` (start over), `fmi2FreeInstance`, `fmi2SetFMUstate` (go back to a snapshot), `fmi2GetXXX` and `fmi2SetDebugLogging` (diagnosis), and the functions that only handle snapshots (`fmi2FreeFMUstate`, serialization).
- `fmi2Warning` means the call had its effect, but something is worth reporting. It does not change the state.
- `fmi2Discard` is returned in two cases:
  - by `fmi2DoStep` when the integration fails numerically (a state, a Real output or a Real local variable becomes NaN or infinite). The FMU is rolled back to the communication point and enters the **Step Failed** state (see [`fmi2DoStep`](#fmi2dostep)).
  - by the status queries, for a status that is not available. This does not change the state.
- A NULL component returns `fmi2Error` (there is no instance whose state could change).

---

## Inquiry Functions

These functions provide information about the FMI implementation and must not depend on an FMU instance.

### fmi2GetTypesPlatform

Returns the platform-specific type definitions used by this FMU.

#### Signature
```c
const char* fmi2GetTypesPlatform(void);
```

#### Description
Returns a string identifying the types platform for this FMU binary. This allows integration tools to verify compatibility between the FMU binary and the platform it's running on. The function is stateless and can be called before instantiation.

#### Parameters
None.

#### Return Values
| Return Value | Description |
|--------------|-------------|
| `"default"` or platform identifier | A constant string identifying the platform (typically `"default"` for standard FMI 2.0) |

#### FMI State Requirements
**Can be called in**: Any state, including before instantiation
**State transitions**: None

#### Usage Example
```c
const char* platform = fmi2GetTypesPlatform();
printf("FMI Types Platform: %s\n", platform);
// Typically prints: "default"
```

#### Related Functions
- [`fmi2GetVersion`](#fmi2getversion) - Get FMI version string

#### Notes
- This function is defined by the FMI standard and returns `fmi2TypesPlatform` constant from `fmi2TypesPlatform.h`
- The return value is typically `"default"` for standard FMI 2.0 implementations
- No memory management needed - returns pointer to static string

---

### fmi2GetVersion

Returns the version of the FMI standard implemented by this FMU.

#### Signature
```c
const char* fmi2GetVersion(void);
```

#### Description
Returns the FMI version string that this FMU implementation conforms to. This allows integration tools to verify they can work with this FMU. The function is stateless and can be called before instantiation.

#### Parameters
None.

#### Return Values
| Return Value | Description |
|--------------|-------------|
| `"2.0"` | String indicating FMI 2.0 compliance |

#### FMI State Requirements
**Can be called in**: Any state, including before instantiation
**State transitions**: None

#### Usage Example
```c
const char* version = fmi2GetVersion();
printf("FMI Version: %s\n", version);
// Prints: "2.0"

// Integration tool might check compatibility
if (strcmp(version, "2.0") != 0) {
    fprintf(stderr, "Unsupported FMI version\n");
}
```

#### Related Functions
- [`fmi2GetTypesPlatform`](#fmi2gettypesplatform) - Get types platform identifier

#### Notes
- Returns the constant `fmi2Version` from `fmi2Functions.h`
- Always returns `"2.0"` for FMI 2.0 compliant FMUs
- No memory management needed - returns pointer to static string

---

## Instance Management

Functions for creating, configuring, and destroying FMU instances.

### fmi2Instantiate

Creates a new instance of the FMU with specified configuration and callbacks.

#### Signature
```c
fmi2Component fmi2Instantiate(fmi2String instanceName,
                              fmi2Type fmuType,
                              fmi2String fmuGUID,
                              fmi2String fmuResourceLocation,
                              const fmi2CallbackFunctions* functions,
                              fmi2Boolean visible,
                              fmi2Boolean loggingOn);
```

#### Description
Allocates and initializes a new FMU instance. This is the first function that must be called (after inquiry functions) to create an FMU that can be simulated. It validates the GUID, sets up callbacks, and allocates both the FMI wrapper structure and the underlying model.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `instanceName` | `fmi2String` | Unique name for this FMU instance (for logging/debugging) |
| `fmuType` | `fmi2Type` | Must be `fmi2CoSimulation` (Model Exchange is not supported) |
| `fmuGUID` | `fmi2String` | GUID from modelDescription.xml - must match FMU's GUID |
| `fmuResourceLocation` | `fmi2String` | `file:` URI of the `resources` directory of the unzipped FMU, where the model finds its data files (may be NULL or empty) |
| `functions` | `const fmi2CallbackFunctions*` | Pointer to callback function structure. Only `logger` and `componentEnvironment` are used |
| `visible` | `fmi2Boolean` | Whether simulation should be visible (UI-related, often ignored) |
| `loggingOn` | `fmi2Boolean` | Whether logging should be enabled initially |

#### Return Values
| Return Value | Condition |
|--------------|-----------|
| Valid `fmi2Component` pointer | Successfully instantiated |
| `NULL` | Instantiation failed (missing logger, missing instance name, `fmuType` is not Co-Simulation, GUID mismatch, allocation failure) |

#### FMI State Requirements
**Can be called in**: No instance exists yet (before any state)
**State transitions**: Creates instance in **Instantiated** state

#### Usage Example
```c
// Define callbacks
void myLogger(fmi2ComponentEnvironment env, fmi2String instanceName,
              fmi2Status status, fmi2String category, fmi2String message, ...) {
    printf("[%s] %s\n", category, message);
}

fmi2CallbackFunctions callbacks = {
    .logger = myLogger,
    .allocateMemory = calloc,
    .freeMemory = free,
    .stepFinished = NULL,  // Not needed for basic co-sim
    .componentEnvironment = NULL
};

// Instantiate the FMU
fmi2Component fmu = fmi2Instantiate(
    "myFMU",                                    // instance name
    fmi2CoSimulation,                           // type
    "{a1b2c3d4-e5f6-7890-abcd-ef1234567890}",  // GUID from modelDescription.xml
    "file:///path/to/resources",                // resource location
    &callbacks,                                 // callbacks
    fmi2False,                                  // not visible
    fmi2True                                    // logging on
);

if (fmu == NULL) {
    fprintf(stderr, "Failed to instantiate FMU\n");
    exit(1);
}
```

#### Related Functions
- [`fmi2FreeInstance`](#fmi2freeinstance) - Destroy the FMU instance
- [`fmi2SetupExperiment`](#fmi2setupexperiment) - Next step after instantiation

#### Notes
- **GUID validation is mandatory** - the provided `fmuGUID` must exactly match the `guid` of the model definition (`MODEL_DEFINITION` in the model's `variables.c`). The generated `modelDescription.xml` carries the same value
- **Only the logger is required**: `functions` and `functions->logger` must not be NULL. `componentEnvironment` is passed back to the logger unchanged and may be NULL
- **Memory callbacks are not used**: The FMU allocates with `calloc()` and frees with `free()`. This is announced by `canNotUseMemoryManagementFunctions="true"` in the `modelDescription.xml`, so `allocateMemory` and `freeMemory` may be NULL
- **The instance name is copied**: The caller may free its string after the call. The name must not be NULL or empty
- **Resource location**: The model gets the `resources` directory as a plain path (`model->resourcePath`, ending with `/`). FMI 2.0 requires a `file` URI with an absolute path, "either including or omitting the authority component" (section 2.1.5), so all of these are understood: `file:///C:/temp/MyFMU/resources`, `file:/C:/temp/MyFMU/resources`, `file:///home/me/MyFMU/resources` and `file://localhost/home/me/MyFMU/resources`. Percent-encoded characters (`%20` for a space, `%C3%A4` for `ä`) are decoded, and all other characters are taken as they are, also a space, `#` or `?` that the integration tool did not encode (`file:///home/me/C# projects/resources` works as well); the scheme is case-insensitive
- **No resource location is no error**: NULL or an empty string leave the model without resource folder. So does any other location (another scheme, another host, a relative path, a malformed `%` escape), which is also reported as a warning. Only a model that reads data files needs the folder; it then fails in [`fmi2ExitInitializationMode`](#fmi2exitinitializationmode) with a message in the log
- **The resource folder is logged**: With logging on, the instantiation reports the decoded folder (`logEvents`)
- Internally allocates both `myFMU2` wrapper and `modelInstance` using `newModel(&MODEL_DEFINITION)`
- **All variables hold their start values** after instantiation (parameters, inputs and states as declared in the model's variable table). Values set afterwards with `fmi2SetReal` etc. are kept through initialization
- Returns an opaque `fmi2Component` pointer - integration tool should never access internals directly
- **Only the `fmi2*` functions are exported** from the binary. `MODEL_DEFINITION` and all other internals are hidden, so several FMUs built from this template can be loaded into one process
- If instantiation fails, logs error via the logger callback before returning NULL (independent of `loggingOn`, category `logStatusError`)

---

### fmi2FreeInstance

Frees an FMU instance and all associated memory.

#### Signature
```c
void fmi2FreeInstance(fmi2Component c);
```

#### Description
Destroys an FMU instance and releases all allocated memory. This is the final cleanup function and should be called when the integration tool is done with the FMU. Safe to call with NULL pointer.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance to free (can be NULL) |

#### Return Values
This function returns `void` (no return value).

#### FMI State Requirements
**Can be called in**: Any state after instantiation
**State transitions**: Instance is destroyed (no longer exists)

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);

// ... use the FMU ...

// Clean up when done
fmi2FreeInstance(fmu);
fmu = NULL;  // Good practice to avoid dangling pointer
```

#### Related Functions
- [`fmi2Instantiate`](#fmi2instantiate) - Creates the instance that this function frees

#### Notes
- **NULL-safe**: Can be called with `c = NULL` without crashing
- Frees both the underlying `modelInstance` (via `freeModel()`) and the `myFMU2` wrapper
- Should be called exactly once for each successful `fmi2Instantiate` call
- Does not require prior call to `fmi2Terminate` (though recommended)

---

### fmi2SetupExperiment

Configures the simulation experiment with time bounds and tolerances.

#### Signature
```c
fmi2Status fmi2SetupExperiment(fmi2Component c,
                               fmi2Boolean toleranceDefined,
                               fmi2Real tolerance,
                               fmi2Real startTime,
                               fmi2Boolean stopTimeDefined,
                               fmi2Real stopTime);
```

#### Description
Informs the FMU about the simulation time window and numerical tolerances before initialization. This must be called after instantiation and before entering initialization mode. The FMU uses this information to set its internal clock and prepare for the simulation experiment.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `toleranceDefined` | `fmi2Boolean` | Whether `tolerance` parameter is meaningful |
| `tolerance` | `fmi2Real` | Relative tolerance for numerical integration, stored as `model->relTol`. Used by adaptive integrators; the fixed-step integrators (`EULER_FORWARD`, `RUNGE_KUTTA_4`) ignore it |
| `startTime` | `fmi2Real` | Start time of the experiment in seconds |
| `stopTimeDefined` | `fmi2Boolean` | Whether `stopTime` parameter is meaningful |
| `stopTime` | `fmi2Real` | Stop time of the experiment in seconds |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Experiment setup successful |
| `fmi2Warning` | Experiment set up, but the defined `tolerance` is not finite or not > 0. It is ignored and the default tolerance applies |
| `fmi2Error` | Invalid time window (`startTime` not finite, `stopTime < startTime`) or wrong state |

#### FMI State Requirements
**Can be called in**: **Instantiated** state (after `fmi2Instantiate`, before `fmi2EnterInitializationMode`)
**State transitions**: Remains in **Instantiated** state

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);

// Setup a simulation from t=0 to t=10 seconds
fmi2Status status = fmi2SetupExperiment(
    fmu,
    fmi2False,  // tolerance not defined: the model's default applies
    0.0,        // tolerance (not read if undefined)
    0.0,        // start time
    fmi2True,   // stop time is defined
    10.0        // stop time = 10 seconds
);

if (status != fmi2OK) {
    fprintf(stderr, "Setup experiment failed\n");
}
```

#### Related Functions
- [`fmi2Instantiate`](#fmi2instantiate) - Must be called before this
- [`fmi2EnterInitializationMode`](#fmi2enterinitializationmode) - Next step after setup

#### Notes
- Must be called before `fmi2EnterInitializationMode`
- **Tolerance**: a defined `tolerance` becomes `model->relTol`. One that is not finite and greater than zero is ignored: the function returns `fmi2Warning` (logged in `logStatusWarning`), and the default applies as if `toleranceDefined` were false. FMI 2.0 defines no range for the tolerance and allows a Co-Simulation FMU to ignore it, so this is no reason to fail. If `toleranceDefined` is false, `tolerance` is not read and the model's default applies: `defaultExperiment.tolerance` of the model definition, or `1e-6` (`MODEL_DEFAULT_TOLERANCE`) if the model suggests none. The last call before `fmi2EnterInitializationMode` counts, and `fmi2Reset` restores the default
- The start time, stop time and tolerance are written to the log (category `logEvents`)
- Internally stores `startTime`, `stopTime`, and initializes the internal clock to `startTime`. The clock is the model's time `model->time`, so the equations `f(t,x,u)` and `h(t,x,u)` see `t = startTime` during initialization. Outputs are recomputed with the new time when they are read next
- If `stopTimeDefined` is true, `stopTime` must be `>= startTime`, and [`fmi2DoStep`](#fmi2dostep) later refuses to step beyond it. If it is false, `stopTime` is ignored
- `startTime` must be finite
- The time values are in seconds (SI units as per FMI standard)

---

### fmi2SetDebugLogging

Controls which log categories are active during simulation.

#### Signature
```c
fmi2Status fmi2SetDebugLogging(fmi2Component c,
                               fmi2Boolean loggingOn,
                               size_t nCategories,
                               const fmi2String categories[]);
```

#### Description
Enables or disables logging and controls which message categories are logged. The integration tool calls this to adjust logging verbosity dynamically during simulation - for example, enabling detailed logging only when debugging specific issues, or disabling all logging for performance in production.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `loggingOn` | `fmi2Boolean` | `fmi2True` to enable logging, `fmi2False` to disable all logging |
| `nCategories` | `size_t` | Number of category strings in the `categories` array |
| `categories` | `const fmi2String[]` | Array of category names to enable (can be `NULL` if `nCategories == 0`) |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Logging configuration updated successfully |
| `fmi2Warning` | Configuration updated, but at least one category name is unknown (or NULL) and was ignored |
| `fmi2Error` | NULL component or NULL categories with nCategories > 0 |

#### FMI State Requirements
**Can be called in**: Any state after instantiation, including the Error state
**State transitions**: None (configuration function only)

#### Behavior

The function has three distinct behaviors based on parameters:

1. **Disable all logging** (`loggingOn == fmi2False`):
   - Ignores `nCategories` and `categories`
   - No log messages will be produced

2. **Enable all categories** (`loggingOn == fmi2True` and `nCategories == 0`):
   - `categories` can be `NULL`
   - All log categories become active

3. **Enable specific categories** (`loggingOn == fmi2True` and `nCategories > 0`):
   - Only the specified categories become active
   - Unknown category names are ignored; the function then returns `fmi2Warning`

#### Log Categories

The FMU has the following categories. They are listed as `LogCategories` in the `modelDescription.xml`, and the same name is passed as `category` to the logger callback.

| Category | Messages |
|----------|----------|
| `logEvents` | Lifecycle of the FMU: instantiated, experiment set up, initialized, reset, terminated, FMU state saved or restored |
| `logStatusWarning` | A function returned `fmi2Warning` or `fmi2Discard` |
| `logStatusError` | A function returned `fmi2Error` (the message tells why) |
| `logFmi2Call` | Successful calls of `fmi2GetXXX`, `fmi2SetXXX` and `fmi2DoStep` (very verbose) |
| `logAll` | All of the above |

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);

// Example 1: Enable only error logging
fmi2String errorOnly[] = {"logStatusError"};
fmi2SetDebugLogging(fmu, fmi2True, 1, errorOnly);

// Example 2: Enable all logging for debugging
fmi2SetDebugLogging(fmu, fmi2True, 0, NULL);  // nCategories=0 means all

// Example 3: Enable specific categories
fmi2String categories[] = {"logStatusError", "logEvents"};
fmi2SetDebugLogging(fmu, fmi2True, 2, categories);

// Example 4: Disable all logging for performance
fmi2SetDebugLogging(fmu, fmi2False, 0, NULL);

// Example 5: Change logging during simulation
fmi2DoStep(fmu, 0.0, 0.1, fmi2True);
// ... enable verbose logging for next step
fmi2String verbose[] = {"logAll"};
fmi2SetDebugLogging(fmu, fmi2True, 1, verbose);
fmi2DoStep(fmu, 0.1, 0.1, fmi2True);
```

#### Related Functions
- [`fmi2Instantiate`](#fmi2instantiate) - Logger callback is set during instantiation
- All functions that log messages use the active categories to filter output

#### Notes
- **Implementation uses bitmask**: Categories are stored as bit flags for efficient checking (`unsigned int logLevel`)
- **One table of categories**: `LOG_CATEGORIES` in `fmi2Functions.c` maps names to bits. It is used by this function and by the logger, so the names cannot diverge
- **Dynamic control**: Integration tools can enable verbose logging only during problematic simulation periods
- **Categories parameter can be NULL**: When `nCategories == 0`, NULL is valid and means "enable all"
- **No default category**: With `loggingOn = fmi2False` nothing is logged, not even errors. Only a failed `fmi2Instantiate` always reports its reason
- **Messages are passed as data**: The logger callback is printf-like. The FMU formats its message first and calls the logger with the format `"%s"`, so a `%` in a variable or instance name is never interpreted

---

## Initialization

Functions for initializing the FMU with initial conditions before simulation begins.

### fmi2EnterInitializationMode

Transitions the FMU into initialization mode where initial values can be set.

#### Signature
```c
fmi2Status fmi2EnterInitializationMode(fmi2Component c);
```

#### Description
Moves the FMU from the instantiated state into initialization mode. The function does not change any variable: all variables received their start values in `fmi2Instantiate` (or `fmi2Reset`), and values the integration tool has set since then are kept. The integration tool can continue to set parameters, inputs and states (except states published with `initial="approx"`) by calling `fmi2SetReal`, `fmi2SetInteger`, etc. before calling `fmi2ExitInitializationMode` to finalize initialization. In initialization mode, reading a variable after a set initializes the model for the values set so far, so the outputs and calculated parameters read there are those the simulation starts from (FMI 2.0, section 4.2.4: in Initialization Mode "equations are active to determine all outputs").

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Successfully entered initialization mode |
| `fmi2Error` | NULL component or FMU not in instantiated state |

#### FMI State Requirements
**Can be called in**: **Instantiated** state (after `fmi2SetupExperiment`, before first `fmi2ExitInitializationMode`)
**State transitions**: **Instantiated** → **Initialization Mode**

#### Where Initial Values Come From

| Step | What happens to the variables |
|------|-------------------------------|
| `fmi2Instantiate` / `fmi2Reset` | Every variable is set to the start value declared in the model's variable table |
| Instantiated state | Integration tool may set parameters, inputs and states |
| `fmi2EnterInitializationMode` | Nothing (values are kept). A new initialization begins: the states with `initial="approx"` hold the guesses that every initialization starts from |
| Initialization mode | Integration tool may set parameters, inputs and states with `initial="exact"`. A read after a set initializes the model for the current values (calculated parameters, its initialize function, outputs) |
| `fmi2ExitInitializationMode` | The model is initialized for the current values (unless a read already did so and nothing was set since), then the outputs are computed from the initial values |

#### Usage Example
```c
/* value references of the first_order model (position in its variable table) */
const fmi2ValueReference VR_tau = 0, VR_k = 1, VR_x = 4;

fmi2Component fmu = fmi2Instantiate(/* ... */);

// Setup the experiment
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);

// Enter initialization mode
fmi2Status status = fmi2EnterInitializationMode(fmu);
if (status != fmi2OK) {
    fprintf(stderr, "Failed to enter initialization mode\n");
    return -1;
}

// Override default initial state (optional)
fmi2Real initialState = 5.0;
fmi2SetReal(fmu, &VR_x, 1, &initialState);

// Override default parameters (optional)
fmi2ValueReference paramRefs[] = {VR_tau, VR_k};
fmi2Real paramValues[] = {0.2, 2.0};
fmi2SetReal(fmu, paramRefs, 2, paramValues);

// Finalize initialization
fmi2ExitInitializationMode(fmu);
```

#### Related Functions
- [`fmi2SetupExperiment`](#fmi2setupexperiment) - Must be called before this
- [`fmi2ExitInitializationMode`](#fmi2exitinitializationmode) - Finalizes initialization
- [`fmi2SetReal`](#fmi2setreal) - Can be called during initialization to override defaults
- [`fmi2GetReal`](#fmi2getreal) - Can be called during initialization to read current values

#### Notes
- **Start values are not applied here**: They are written once, in `fmi2Instantiate` and `fmi2Reset`. Applying them here would overwrite values the integration tool has set in the instantiated state (this is where fmpy applies `start_values`)
- **Must be instantiated first**: Returns `fmi2Error` if not called in the instantiated state
- **`fmi2SetupExperiment` is optional for this FMU**: Without it the experiment starts at time 0 and has no stop time

---

### fmi2ExitInitializationMode

Finalizes initialization and transitions to step mode, ready for simulation.

#### Signature
```c
fmi2Status fmi2ExitInitializationMode(fmi2Component c);
```

#### Description
Completes the initialization phase and transitions the FMU from initialization mode to initialized/step mode. First the model is initialized for the current values: its calculated parameters are computed, then its initialize function runs (if it has one), which may replace the start values of states published with `initial="approx"` (e.g. by a steady state). If a read in initialization mode has already initialized the model and nothing was set since, that initialization stands. Then the outputs are computed from the initial states, inputs and parameters, so they are valid at the first communication point. After this function returns successfully, the FMU is ready to execute simulation steps via `fmi2DoStep`. This is the final step before beginning the simulation loop. No more initialization-time variable setting is allowed after this call.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Successfully exited initialization mode and ready for stepping |
| `fmi2Error` | NULL component, FMU not in initialization mode, or the model cannot start: its initialize function reported an error (the model's reason is logged, e.g. `Initialization failed: init_mode 3 is neither 1 (fixed_start) nor 2 (steady_state).`) or left a state that is not finite, or a Real output or local variable is not finite at the start (e.g. `Initialization failed: Output 'u' is not finite.`). The FMU enters the Error state |

#### FMI State Requirements
**Can be called in**: **Initialization Mode** (after `fmi2EnterInitializationMode`)
**State transitions**: **Initialization Mode** → **Step Complete**

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);

// Enter initialization and optionally set custom values
fmi2EnterInitializationMode(fmu);
fmi2Real customState = 5.0;
fmi2ValueReference stateRef = 0;
fmi2SetReal(fmu, &stateRef, 1, &customState);

// Finalize initialization
fmi2Status status = fmi2ExitInitializationMode(fmu);
if (status != fmi2OK) {
    fprintf(stderr, "Failed to exit initialization mode\n");
    return -1;
}

// FMU is now ready for simulation
fmi2Real output;
fmi2ValueReference outRef = 0;

// Simulation loop
for (double t = 0.0; t < 10.0; t += 0.01) {
    fmi2DoStep(fmu, t, 0.01, fmi2True);
    fmi2GetReal(fmu, &outRef, 1, &output);
    printf("t=%.2f, y=%.4f\n", t, output);
}
```

#### Related Functions
- [`fmi2EnterInitializationMode`](#fmi2enterinitializationmode) - Must be called before this
- [`fmi2DoStep`](#fmi2dostep) - Next step after exiting initialization
- [`fmi2GetReal`](#fmi2getreal) - Can be called after this to read outputs
- [`fmi2SetReal`](#fmi2setreal) - Can be called during initialization before this

#### Notes
- **Initial outputs**: Calls the model's output equation once, so that `fmi2GetReal` returns y(t0) = h(x0, u0) right after initialization
- **Initialization for the current values**: Calculated parameters, then the model's initialize function, then the outputs. A read in initialization mode does the same, so this call only initializes again if something was set since the last read. Every initialization starts from the same guesses (the states with `initial="approx"` as they were when initialization mode began), so the result does not depend on how often the integration tool read. A failure is `fmi2Error` (FMI 2.0 has no way to repeat this call, so there is no `fmi2Discard`)
- **Data files are read in the initialization**: A model reads its resources in its initialize function. A file that is missing or invalid fails this call (or an earlier read in initialization mode), and the log names the file by its full path
- **States and fixed parameters are frozen**: After this call, states and parameters declared as `VAR_FIXED` can no longer be set
- **Finite start**: The states, and the Real outputs and local variables, must be finite at the start; the master starts from them, and every step from a non-finite start would be discarded
- **Parameter values are checked by the model**: A model refuses values its equations cannot run with in its initialize function, e.g. `thermal_mass` with R = 0 (the bounds `min`/`max` include their limits)
- **Finalization point**: After this call, `fmi2EnterInitializationMode` cannot be called again without calling `fmi2Reset` first
- **Ready for stepping**: Integration tool typically reads initial outputs immediately after this call, then begins the simulation loop
- **Error if wrong state**: Returns `fmi2Error` if not currently in initialization mode (catches lifecycle violations)

---

## Variable Access

Functions for setting and reading variable values during initialization and simulation.

### Value References

Every variable of the model is described by one entry in the model's variable table (`VARIABLES[]` in `src/models/<model_name>/variables.c`). **The value reference of a variable is the index of its entry in that table.** The `modelDescription.xml` is generated from the same table, so both always agree.

All access functions below work the same way:

1. The value reference is looked up in the variable table
2. The type of the variable must match the function (`fmi2SetReal` only accepts Real variables, and so on)
3. For setters, the variable must be settable in the current state:

| Role | Can be set |
|------|------------|
| Input | Instantiated, Initialization Mode, Step Complete |
| Parameter (`VAR_TUNABLE`, default) | Instantiated, Initialization Mode, Step Complete |
| Parameter (`VAR_FIXED`) | Instantiated, Initialization Mode |
| State | Instantiated, Initialization Mode |
| State with `initial="approx"` | Instantiated |
| Calculated parameter | Never |
| Output | Never |
| Local variable | Never |

Integration tools should always look up value references by variable name in the `modelDescription.xml` and never hardcode them.

**Outputs, local variables and calculated parameters are always consistent with the other variables.** Every setter, and `fmi2SetupExperiment` (which moves the time), marks them as stale, and every getter recomputes them first if needed (by calling the model's calculateParameters function, then its output equation, which computes outputs and local variables). `fmi2DoStep` also recomputes the calculated parameters first if a variable was set since the last step. Calculated parameters are listed as `InitialUnknowns` in the `ModelStructure`. Outputs can therefore be read before and during initialization, and a model with direct feedthrough (`y` depends on `u`) answers with the inputs set last. Which inputs an output depends on is declared by the model (`.dependsOn`) and published in the `ModelStructure` of the `modelDescription.xml`. The `pi_controller` model has outputs with direct feedthrough: `u` and `e` change as soon as `r` or `y_m` is set.

**Example for the `first_order` model:**

| Value reference | Name | Role | Type |
|-----------------|------|------|------|
| 0 | `tau` | parameter | Real |
| 1 | `k` | parameter | Real |
| 2 | `u` | input | Real |
| 3 | `y` | output | Real |
| 4 | `x` | state | Real |

---

### fmi2SetReal

Sets real-valued variables (parameters, inputs, states).

#### Signature
```c
fmi2Status fmi2SetReal(fmi2Component c,
                       const fmi2ValueReference vr[],
                       size_t nvr,
                       const fmi2Real value[]);
```

#### Description
Sets one or more real-valued variables by their value references. This is the primary mechanism for the integration tool to provide input signals, configure parameters and set initial conditions.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `vr` | `const fmi2ValueReference[]` | Array of value references identifying which variables to set |
| `nvr` | `size_t` | Number of value references (length of `vr` and `value` arrays) |
| `value` | `const fmi2Real[]` | Array of values to assign to the corresponding variables |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | All variables set successfully |
| `fmi2Error` | NULL component, call not allowed in the current state, invalid arrays, unknown value reference, variable is not of type Real, variable is an output or a local variable, or variable is a state or fixed parameter and the initialization has finished |

#### FMI State Requirements
**Can be called in**: **Instantiated**, **Initialization Mode**, **Step Complete**
**State transitions**: None on success. On error the FMU enters the error state

#### Usage Example
```c
/* value references of the first_order model (position in its variable table) */
const fmi2ValueReference VR_tau = 0, VR_k = 1, VR_u = 2, VR_x = 4;

fmi2Component fmu = fmi2Instantiate(/* ... */);
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);
fmi2EnterInitializationMode(fmu);

// Set parameters during initialization
fmi2ValueReference paramRefs[] = {VR_tau, VR_k};
fmi2Real paramValues[] = {0.2, 2.5};
fmi2Status status = fmi2SetReal(fmu, paramRefs, 2, paramValues);
if (status != fmi2OK) {
    fprintf(stderr, "Failed to set parameters\n");
}

// Set the initial condition of the state
fmi2Real x0 = 1.0;
fmi2SetReal(fmu, &VR_x, 1, &x0);

fmi2ExitInitializationMode(fmu);

// Set input during simulation
fmi2Real inputValue = 1.0;
fmi2SetReal(fmu, &VR_u, 1, &inputValue);

// Attempt to set a state after initialization (ERROR)
status = fmi2SetReal(fmu, &VR_x, 1, &x0);  // Returns fmi2Error
```

#### Related Functions
- [`fmi2GetReal`](#fmi2getreal) - Read real-valued variables
- [`fmi2SetInteger / fmi2GetInteger`](#fmi2setinteger--fmi2getinteger) - Same for integer variables
- [`fmi2SetBoolean / fmi2GetBoolean`](#fmi2setboolean--fmi2getboolean) - Same for boolean variables
- [`fmi2DoStep`](#fmi2dostep) - Use inputs set via this function

#### Notes
- **Bulk operations**: Arrays allow setting multiple variables in one call for efficiency
- **Model independent**: The function only uses the variable table of the model definition; see [Value References](#value-references)
- **States are initial conditions**: A state can be set until `fmi2ExitInitializationMode`. Afterwards the states are owned by the integrator
- **nvr=0 is valid**: Calling with empty arrays is a no-op, not an error
- **Error handling**: Returns error on the first invalid value reference (fail-fast pattern). Variables earlier in the array have already been set

---

### fmi2GetReal

Reads real-valued variables (parameters, inputs, outputs, states).

#### Signature
```c
fmi2Status fmi2GetReal(fmi2Component c,
                       const fmi2ValueReference vr[],
                       size_t nvr,
                       fmi2Real value[]);
```

#### Description
Reads one or more real-valued variables by their value references. Every real variable of the model can be read. Integration tools typically use this after `fmi2DoStep` to read output values. In initialization mode, the model is first initialized for the values set so far (if something was set since the last read), so the outputs read there are those the simulation starts from.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `vr` | `const fmi2ValueReference[]` | Array of value references identifying which variables to read |
| `nvr` | `size_t` | Number of value references (length of `vr` and `value` arrays) |
| `value` | `fmi2Real[]` | **Output array** where values will be written |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | All variables read successfully |
| `fmi2Error` | NULL component, call not allowed in the current state, invalid arrays, unknown value reference, or variable is not of type Real. In initialization mode also if the model cannot be initialized for the current values: the model's reason is logged, as `fmi2ExitInitializationMode` would log it (e.g. `Initialization failed: R and C must be greater than zero (R = 0 K/W, C = 1000 J/K).`) |

#### FMI State Requirements
**Can be called in**: **Instantiated**, **Initialization Mode**, **Step Complete**, **Step Failed**, **Step Canceled**, **Terminated**, **Error**
**State transitions**: None on success. On error the FMU enters the error state

#### Usage Example
```c
/* value references of the first_order model (position in its variable table) */
const fmi2ValueReference VR_tau = 0, VR_k = 1, VR_u = 2, VR_y = 3;

fmi2Component fmu = fmi2Instantiate(/* ... */);

// Read back parameters to verify
fmi2ValueReference paramRefs[] = {VR_tau, VR_k};
fmi2Real paramValues[2];
fmi2GetReal(fmu, paramRefs, 2, paramValues);
printf("tau=%.3f, k=%.3f\n", paramValues[0], paramValues[1]);

fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);
fmi2EnterInitializationMode(fmu);
fmi2ExitInitializationMode(fmu);

// Simulation loop: set input, step, read output
fmi2Real input = 1.0, output;

for (double t = 0.0; t < 10.0; t += 0.01) {
    fmi2SetReal(fmu, &VR_u, 1, &input);
    fmi2DoStep(fmu, t, 0.01, fmi2True);
    fmi2GetReal(fmu, &VR_y, 1, &output);
    printf("t=%.2f, u=%.2f, y=%.4f\n", t, input, output);
}
```

#### Related Functions
- [`fmi2SetReal`](#fmi2setreal) - Set real-valued variables
- [`fmi2DoStep`](#fmi2dostep) - Updates outputs based on inputs

#### Notes
- **Output array is written**: The `value[]` parameter is an output, caller must allocate space for `nvr` elements
- **Outputs are always up to date**: They are computed in `fmi2ExitInitializationMode` and after every `fmi2DoStep`, and again before reading if a variable was set in between; see [Value References](#value-references)
- **Readable in the Error state**: Useful to inspect the FMU after a failure. The values are those of the moment the error occurred
- **Model independent**: The function only uses the variable table of the model definition; see [Value References](#value-references)
- **Bulk operations**: Read multiple variables in one call for efficiency
- **nvr=0 is valid**: Calling with empty arrays is a no-op
- **All roles readable**: Unlike `fmi2SetReal`, this can read outputs as well

---

### fmi2SetInteger / fmi2GetInteger

Sets and reads integer-valued variables.

#### Signature
```c
fmi2Status fmi2SetInteger(fmi2Component c,
                          const fmi2ValueReference vr[],
                          size_t nvr,
                          const fmi2Integer value[]);

fmi2Status fmi2GetInteger(fmi2Component c,
                          const fmi2ValueReference vr[],
                          size_t nvr,
                          fmi2Integer value[]);
```

#### Description
The integer counterparts of `fmi2SetReal` and `fmi2GetReal`. A variable is an integer variable if its field in the model's struct is of type `int`. Enumeration variables (`<Enumeration declaredType="...">` in the `modelDescription.xml`) are integer variables too and use the same two functions; only the values of the items of their type can be set.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `vr` | `const fmi2ValueReference[]` | Array of value references identifying the variables |
| `nvr` | `size_t` | Number of value references (length of `vr` and `value` arrays) |
| `value` | `fmi2Integer[]` | Values to set (`const`, for the setter) or **output array** (for the getter) |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | All variables set / read successfully |
| `fmi2Error` | Same conditions as `fmi2SetReal` / `fmi2GetReal`; the variable must be of type Integer or Enumeration. For an enumeration variable, `fmi2SetInteger` also fails if the value is not the value of an item of its type (e.g. `Value 3 of variable 'init_mode' is not an item of the enumeration 'InitMode'.`); the variable keeps its value |

#### FMI State Requirements
**Can be called in**: Same states as [`fmi2SetReal`](#fmi2setreal) and [`fmi2GetReal`](#fmi2getreal)
**State transitions**: None on success. On error the FMU enters the error state

#### Usage Example
```c
/* value references of the typed_demo model (position in its variable table) */
const fmi2ValueReference VR_level = 5, VR_boost = 6;

// Double the ramp rate
fmi2Integer boost = 2;
fmi2SetInteger(fmu, &VR_boost, 1, &boost);

fmi2DoStep(fmu, time, 0.01, fmi2True);

fmi2Integer level;
fmi2GetInteger(fmu, &VR_level, 1, &level);
```

#### Related Functions
- [`fmi2SetReal`](#fmi2setreal) / [`fmi2GetReal`](#fmi2getreal) - Same for real variables
- [`fmi2SetBoolean / fmi2GetBoolean`](#fmi2setboolean--fmi2getboolean) - Same for boolean variables

#### Notes
- **Same rules as for Real variables**: See [Value References](#value-references) for which variables can be set when
- **Discrete in the model description**: Integer and enumeration inputs and outputs are declared with `variability="discrete"`, as required by FMI 2.0
- **Enumerations**: The items of every enumeration type are listed in the `TypeDefinitions` of the `modelDescription.xml`. Values that are no item are refused, also values between two items. The `thermal_mass` model has the enumeration parameter `init_mode` (type `InitMode`)

---

### fmi2SetBoolean / fmi2GetBoolean

Sets and reads boolean-valued variables.

#### Signature
```c
fmi2Status fmi2SetBoolean(fmi2Component c,
                          const fmi2ValueReference vr[],
                          size_t nvr,
                          const fmi2Boolean value[]);

fmi2Status fmi2GetBoolean(fmi2Component c,
                          const fmi2ValueReference vr[],
                          size_t nvr,
                          fmi2Boolean value[]);
```

#### Description
The boolean counterparts of `fmi2SetReal` and `fmi2GetReal`. A variable is a boolean variable if its field in the model's struct is of type `bool`.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `vr` | `const fmi2ValueReference[]` | Array of value references identifying the variables |
| `nvr` | `size_t` | Number of value references (length of `vr` and `value` arrays) |
| `value` | `fmi2Boolean[]` | Values to set (`const`, for the setter) or **output array** (for the getter) |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | All variables set / read successfully |
| `fmi2Error` | Same conditions as `fmi2SetReal` / `fmi2GetReal`; the variable must be of type Boolean |

#### FMI State Requirements
**Can be called in**: Same states as [`fmi2SetReal`](#fmi2setreal) and [`fmi2GetReal`](#fmi2getreal)
**State transitions**: None on success. On error the FMU enters the error state

#### Usage Example
```c
/* value references of the typed_demo model (position in its variable table) */
const fmi2ValueReference VR_limit_reached = 0, VR_enable = 1;

// Stop the ramp
fmi2Boolean enable = fmi2False;
fmi2SetBoolean(fmu, &VR_enable, 1, &enable);

fmi2Boolean limitReached;
fmi2GetBoolean(fmu, &VR_limit_reached, 1, &limitReached);
if (limitReached) {
    /* ... */
}
```

#### Related Functions
- [`fmi2SetReal`](#fmi2setreal) / [`fmi2GetReal`](#fmi2getreal) - Same for real variables
- [`fmi2SetInteger / fmi2GetInteger`](#fmi2setinteger--fmi2getinteger) - Same for integer variables

#### Notes
- **Type conversion**: `fmi2Boolean` is an `int` (`fmi2True` = 1, `fmi2False` = 0), the model stores a C `bool`. Any non-zero value is stored as `true`; the getter returns exactly `fmi2True` or `fmi2False`
- **Discrete in the model description**: Boolean inputs and outputs are declared with `variability="discrete"`, as required by FMI 2.0

---

### fmi2SetString / fmi2GetString

Set and get string parameters.

#### Signature
```c
fmi2Status fmi2SetString(fmi2Component c,
                         const fmi2ValueReference vr[],
                         size_t nvr,
                         const fmi2String value[]);

fmi2Status fmi2GetString(fmi2Component c,
                         const fmi2ValueReference vr[],
                         size_t nvr,
                         fmi2String value[]);
```

#### Description
The string counterparts of `fmi2SetReal` and `fmi2GetReal`. A variable is a string variable if its field in the model's `Params` struct is a `char name[MODEL_STRING_SIZE]`; strings are always parameters. A string holds at most `MODEL_STRING_SIZE - 1` (255) bytes of UTF-8 text.

`fmi2SetString` copies every text into the FMU, so the integration tool may free or reuse its strings after the call. `fmi2GetString` returns pointers to the texts inside the FMU instance.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `vr` | `const fmi2ValueReference[]` | Array of value references identifying the variables |
| `nvr` | `size_t` | Number of value references (length of `vr` and `value` arrays) |
| `value` | `fmi2String[]` | Texts to set (`const`, for the setter) or **output array** of pointers (for the getter) |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | All variables set / read successfully |
| `fmi2Error` | Same conditions as `fmi2SetReal` / `fmi2GetReal`; the variable must be of type String. For the setter also: a text is `NULL`, or a text has more than 255 bytes |

#### FMI State Requirements
**Can be called in**: Same states as [`fmi2SetReal`](#fmi2setreal) and [`fmi2GetReal`](#fmi2getreal). A fixed string can only be set until `fmi2ExitInitializationMode`, a tunable one also between steps
**State transitions**: None on success. On error the FMU enters the error state

#### Usage Example
```c
/* value reference of the lookup_table_1d model (position in its variable table) */
const fmi2ValueReference VR_table_file = 0;

// Choose the table before the initialization
fmi2String file = "measured.csv";
fmi2SetString(fmu, &VR_table_file, 1, &file);

// Read it back; copy the text if it is needed after the next FMI call
fmi2String current;
fmi2GetString(fmu, &VR_table_file, 1, &current);
char copy[256];
snprintf(copy, sizeof(copy), "%s", current);
```

#### Related Functions
- [`fmi2SetReal`](#fmi2setreal) / [`fmi2GetReal`](#fmi2getreal) - Same for real variables
- [`fmi2SetInteger / fmi2GetInteger`](#fmi2setinteger--fmi2getinteger) - Same for integer variables

#### Notes
- **Never cut**: A text that does not fit is refused with `fmi2Error` and a log message that names the variable; the variable keeps its value. The limit counts bytes, so a text with non-ASCII characters (two or more bytes each in UTF-8) fits fewer characters
- **Pointer lifetime**: The pointers returned by `fmi2GetString` point into the FMU instance and stay valid until `fmi2FreeInstance`, but the text they point to changes when the variable is set, reset (`fmi2Reset`) or restored (`fmi2SetFMUstate`). FMI 2.0 requires the integration tool to copy the text before the next call to the FMU anyway (section 2.1.7: "The strings returned by fmi2GetString must be copied in the target environment because the allocated memory for these strings might be deallocated by the next call to any of the fmi2 interface functions or it might be an internal string buffer that is reused")
- **Own pointers**: A pointer returned by `fmi2GetString` can be passed back to `fmi2SetString`
- **Part of the FMU state**: The texts are saved, restored and serialized with the FMU state like every other parameter

---

## Simulation

Functions for executing the co-simulation and advancing time.

### fmi2DoStep

Advances the FMU simulation from current time to current time + step size.

#### Signature
```c
fmi2Status fmi2DoStep(fmi2Component c,
                      fmi2Real currentCommunicationPoint,
                      fmi2Real communicationStepSize,
                      fmi2Boolean noSetFMUStatePriorToCurrentPoint);
```

#### Description
The heart of co-simulation - advances the FMU's internal state from time `t` to `t + Δt` by numerically integrating the differential equations. This function is called repeatedly in the simulation loop (potentially millions of times) and is performance-critical. Implements automatic sub-stepping to maintain numerical stability when communication steps are large.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `currentCommunicationPoint` | `fmi2Real` | The current simulation time (must match FMU's internal clock) |
| `communicationStepSize` | `fmi2Real` | Time increment to advance (must be finite and > 0) |
| `noSetFMUStatePriorToCurrentPoint` | `fmi2Boolean` | Flag for FMU state management (ignored in this implementation) |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Step completed successfully |
| `fmi2Warning` | Step completed, but `currentCommunicationPoint` differed from the FMU's internal clock by more than rounding (still within the tolerance). The FMU continued from `currentCommunicationPoint` (see [Time Validation](#time-validation)) |
| `fmi2Discard` | The integration failed numerically (a state, a Real output or a Real local variable became NaN or infinite). The FMU was rolled back to `currentCommunicationPoint` |
| `fmi2Error` | Invalid component, wrong state, step size not finite or not positive, time inconsistency, step ends behind the stop time, or the integrator left `model->time` at another time than the end of the sub-step (see [Algorithm](#algorithm-sub-stepping-for-numerical-stability)) |

#### FMI State Requirements
**Can be called in**: **Step Complete** (after `fmi2ExitInitializationMode`, before `fmi2Terminate`)
**State transitions**: Remains in **Step Complete**. On `fmi2Discard` the FMU enters **Step Failed**, on error the **Error** state

#### Algorithm: Sub-Stepping for Numerical Stability

The step is computed by `simulateTo()` (`include/simulation.h`), the FMI-independent part of the FMU that a standalone simulation calls directly (see `examples/example_model.c`). It integrates from `currentCommunicationPoint` to `tEnd = currentCommunicationPoint + communicationStepSize`, divided into the smallest number of **equal** sub-steps that are not larger than the model's `maxStepSize`:

1. **Calculate sub-steps**:
   ```c
   tEnd           = currentCommunicationPoint + communicationStepSize
   numberOfSteps  = ceil((tEnd - currentCommunicationPoint) / maxStepSize - 1e-9)   // at least 1
   actualStepSize = (tEnd - currentCommunicationPoint) / numberOfSteps
   ```
   The small tolerance keeps rounding in the division from adding a step (`0.3 / 0.1` is `2.9999999999999996` in floating point and must give 3 steps, not 4).
   The step size comes from the interval the clock advances, which can differ from `communicationStepSize` in the last bits (`0.4 - 0.30000000000000004` is `0.09999999999999998`), so the integrated length equals the advance of the clock exactly.
   If `tEnd` rounds to `currentCommunicationPoint` (a step far below the resolution of the time), no sub-step is taken and only the outputs are evaluated.

2. **Remember the communication point**:
   - The internal clock is set to `currentCommunicationPoint` (see [Time Validation](#time-validation))
   - The model data (time, tolerance, variables, states, derivatives, internals, initial guesses) are copied into a buffer that was allocated with the instance (`model->stepStart`), so nothing is allocated during the step

3. **Execute sub-steps**:
   - Take `numberOfSteps` steps of size `actualStepSize`
   - Each step: set `model->time = currentCommunicationPoint + k * actualStepSize` → compute derivatives → integrate states (the integrator sets the stage times itself)
   - After each sub-step: the integrator must have left `model->time` at the end of the sub-step (its contract in `include/model.h`), up to `1e-6` of the sub-step or rounding. Otherwise the integrator is broken, and the function fails with `fmi2Error`. The model keeps the values the integrator left, and the outputs are recomputed from them when they are read
   - After each sub-step: if a state is NaN or infinite, the integration has failed. The model data are restored from the buffer, so the FMU is back at `currentCommunicationPoint` (not at the last good sub-step). The function returns `fmi2Discard` and the FMU enters **Step Failed**

4. **Update time and outputs** (only if all sub-steps succeeded):
   - Set the internal clock (`model->time`) to `tEnd` exactly
   - Compute outputs from final state, so they are evaluated exactly at the communication point
   - If a Real output or a Real local variable is NaN or infinite, the step has failed as well: the model data are restored from the buffer, the function returns `fmi2Discard` and the FMU enters **Step Failed**

**Why sub-stepping?** Explicit integrators like Euler Forward have stability limits. For typical first-order systems, `maxStepSize = 1e-3` ensures stable integration even when the integration tool requests larger communication steps (e.g., 0.1 s).

**Why equal sub-steps?** A full-steps-plus-remainder scheme needs a threshold below which the remainder is dropped, and the remainder step can be arbitrarily small. Equal steps need neither.

#### Discarded Steps

FMI 2.0 (section 4.2.2) lets a slave report with `fmi2Discard` that it could not compute the whole communication step. This FMU does so on numerical failure only, never to stop at an event. Numerical failure means that a state becomes NaN or infinite during a sub-step, or that a Real output or a Real local variable is NaN or infinite at the end of the step. Outputs that are computed between steps (when they are read after a set, or in `fmi2ExitInitializationMode`) are not checked; the next step then discards. The FMU always rolls back to `currentCommunicationPoint`, so the master sees:

| Query | Answer after a discarded step |
|-------|-------------------------------|
| `fmi2GetStatus(fmi2DoStepStatus)` | `fmi2Discard` |
| `fmi2GetRealStatus(fmi2LastSuccessfulTime)` | `currentCommunicationPoint` of the discarded step, also if it differed from the FMU's clock: the integration tool defines the communication points |
| `fmi2GetBooleanStatus(fmi2Terminated)` | `fmi2False`: the step may be repeated, the simulation need not end |
| `fmi2GetReal` etc. | The values at `currentCommunicationPoint` |

In the **Step Failed** state the master can read variables and the status, take or restore a snapshot, terminate, reset or free the FMU. `fmi2DoStep` and the `fmi2SetXXX` functions are not allowed (calling them is an error). To continue, the master restores a snapshot taken at (or before) the start of the failed step, changes what it wants to change (e.g. a tunable parameter), and steps again:

```c
fmi2FMUstate atStart = NULL;
fmi2GetFMUstate(fmu, &atStart);

if (fmi2DoStep(fmu, time, h, fmi2False) == fmi2Discard) {
    fmi2Real lastTime;
    fmi2GetRealStatus(fmu, fmi2LastSuccessfulTime, &lastTime);  /* == time */

    fmi2SetFMUstate(fmu, atStart);     /* back to Step Complete */
    /* ... change a tunable parameter, or retry with another master strategy ... */
    fmi2DoStep(fmu, time, h, fmi2False);
}
fmi2FreeFMUstate(fmu, &atStart);
```

The integrator of this FMU works with sub-steps of a fixed size (see `maxStepSize`), so retrying with a smaller communication step size usually gives the same failure. The `fault_injection` model (`src/models/fault_injection/`) demonstrates the protocol. Its tests in `tests/models/fault_injection/test_discard.py` exercise every allowed and refused call.

#### Time Validation

Before stepping, the function validates:

- **Clock**: `currentCommunicationPoint` must match the FMU's internal clock. Because the clock is set to the end of the step as seen by the integration tool (`currentCommunicationPoint + communicationStepSize`), rounding errors do not accumulate over the steps. A master that advances its time with `t += h` therefore matches the clock exactly, and one that computes `t = t0 + k*h` differs by rounding only. The difference `d = |time - currentCommunicationPoint|` falls into one of three bands:

  | Difference | Result |
  |------------|--------|
  | `d <= max(1e-6 * h, 8 * DBL_EPSILON * max(1, abs(time)))` (rounding, or a negligible fraction of the step) | Accepted silently |
  | `d <= 1e-9 * max(1, abs(time))` | Accepted with `fmi2Warning` and a log message (category `logStatusWarning`). The step starts at `currentCommunicationPoint`, so the FMU continues from the integration tool's clock, and a discarded step rolls back to it ("The calling environment defines the communication points", FMI 2.0, section 4.2.2) |
  | larger | `fmi2Error` |

  The tolerance is relative, so the check also works at large simulation times.
- **Stop time**: If a stop time was defined in `fmi2SetupExperiment`, the step must not end behind it (same tolerance).

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);
fmi2EnterInitializationMode(fmu);
fmi2ExitInitializationMode(fmu);

// Simulation loop
double currentTime = 0.0;
double stopTime = 10.0;
double stepSize = 0.01;  // 10 ms communication steps

fmi2ValueReference inputRef = 2;   // Input u
fmi2ValueReference outputRef = 3;  // Output y
fmi2Real inputValue = 1.0;
fmi2Real outputValue;

while (currentTime < stopTime) {
    // Set input for this step
    fmi2SetReal(fmu, &inputRef, 1, &inputValue);

    // Advance simulation
    fmi2Status status = fmi2DoStep(fmu, currentTime, stepSize, fmi2True);
    if (status != fmi2OK) {
        fprintf(stderr, "DoStep failed at t=%.3f\n", currentTime);
        break;
    }

    // Read output
    fmi2GetReal(fmu, &outputRef, 1, &outputValue);
    printf("t=%.3f, u=%.2f, y=%.4f\n", currentTime, inputValue, outputValue);

    // Advance time
    currentTime += stepSize;
}

fmi2Terminate(fmu);
fmi2FreeInstance(fmu);
```

#### Sub-Stepping Example

**Scenario**: Integration tool requests 0.025 s step, but `maxStepSize = 0.01 s`

**Execution**:
1. Calculate: `numberOfSteps = ceil(2.5) = 3`, `actualStepSize = 0.025 / 3 ≈ 0.00833 s`
2. Step 1 to 3: Integrate 0.00833 s each
3. Check after each sub-step that all states are finite
4. Set clock to `currentCommunicationPoint + 0.025 s`
5. Compute outputs at t+0.025

**Result**: FMU advanced full 0.025 s using three stable sub-steps of equal size.

#### Related Functions
- [`fmi2ExitInitializationMode`](#fmi2exitinitializationmode) - Must be called before first DoStep
- [`fmi2SetReal`](#fmi2setreal) - Set inputs before each DoStep
- [`fmi2GetReal`](#fmi2getreal) - Read outputs after each DoStep
- [`fmi2Terminate`](#fmi2terminate) - End simulation after DoStep loop

#### Notes
- **Performance critical**: Called millions of times in typical simulations - optimized for speed
- **Sub-stepping is transparent**: Integration tool only sees communication steps, internal sub-stepping is hidden
- **Zero-order hold assumption**: Inputs are assumed constant during each communication step (FMI standard)
- **Outputs computed once**: After all sub-steps complete, not after each internal step
- **maxStepSize configurable**: Field of the model definition in the model's `variables.c`
- **Divergence discards the step**: A step that produces NaN or infinite states, Real outputs or Real local variables returns `fmi2Discard` and rolls the FMU back to the communication point (see [Discarded Steps](#discarded-steps)). Continue with `fmi2SetFMUstate`, `fmi2Reset` or `fmi2Terminate`
- **Never pending**: The function returns `fmi2OK`, `fmi2Warning`, `fmi2Discard` or `fmi2Error`, never `fmi2Pending`
- **Synchronous only**: Blocks until computation completes (`noSetFMUStatePriorToCurrentPoint` ignored)
- **No variable step size within DoStep**: Uses fixed sub-steps (adaptive methods would require more complex integrators)

#### Configuration

Adjust in the model definition (`src/models/<model_name>/variables.c`):
```c
    .maxStepSize = 1e-3,  // Maximum stable integration step
```

Smaller values → more stable but slower
Larger values → faster but may diverge for stiff systems

---

### fmi2GetStatus / fmi2GetXXXStatus

Query the status of the co-simulation slave.

#### Signature
```c
fmi2Status fmi2GetStatus       (fmi2Component c, const fmi2StatusKind s, fmi2Status*  value);
fmi2Status fmi2GetRealStatus   (fmi2Component c, const fmi2StatusKind s, fmi2Real*    value);
fmi2Status fmi2GetIntegerStatus(fmi2Component c, const fmi2StatusKind s, fmi2Integer* value);
fmi2Status fmi2GetBooleanStatus(fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value);
fmi2Status fmi2GetStringStatus (fmi2Component c, const fmi2StatusKind s, fmi2String*  value);
```

#### Description
These functions mainly serve slaves that step asynchronously (`fmi2DoStep` returns `fmi2Pending`) or that discard steps (`fmi2DoStep` returns `fmi2Discard`). This FMU never steps asynchronously, but it discards a step on numerical failure. Every status kind of the standard is answered by the function of the matching type:

| Function | Status kind | Value |
|----------|-------------|-------|
| `fmi2GetStatus` | `fmi2DoStepStatus` | Result of the last `fmi2DoStep`: `fmi2Discard` in the **Step Failed** state, otherwise `fmi2OK` (steps are computed synchronously, so the last step is always finished) |
| `fmi2GetRealStatus` | `fmi2LastSuccessfulTime` | End of the last successful step (the internal clock of the FMU). After a discarded step this is the communication point the FMU was rolled back to |
| `fmi2GetBooleanStatus` | `fmi2Terminated` | `fmi2False` (the FMU never asks to stop the simulation; after a discarded step this means that the step may be repeated) |
| `fmi2GetStringStatus` | `fmi2PendingStatus` | `""` (no step is ever pending) |
| `fmi2GetIntegerStatus` | - | FMI 2.0 defines no status of type Integer |

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `s` | `fmi2StatusKind` | Which status to return |
| `value` | pointer | **Output**: the status |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | `value` holds the status |
| `fmi2Discard` | The status kind is not available from this function. The FMU state does not change |
| `fmi2Error` | NULL component, NULL `value` or call not allowed in the current state |

#### FMI State Requirements
**Can be called in**: **Step Complete**, **Step In Progress**, **Step Failed**, **Terminated**
**State transitions**: None on success or `fmi2Discard`. On error the FMU enters the error state

#### Usage Example
```c
fmi2DoStep(fmu, time, 0.01, fmi2True);

fmi2Real lastTime;
if (fmi2GetRealStatus(fmu, fmi2LastSuccessfulTime, &lastTime) == fmi2OK) {
    printf("FMU is at t = %g\n", lastTime);
}
```

#### Related Functions
- [`fmi2DoStep`](#fmi2dostep) - The step these functions report on

#### Notes
- **`fmi2Discard` is not an error**: It only says "no such information". A message is written to the category `logStatusWarning`
- **Derived from the state machine**: `fmi2DoStepStatus` follows the FMU state, so a snapshot taken after a discarded step reports `fmi2Discard` again when it is restored

---

## FMU State

Functions for taking a snapshot of the FMU and returning to it later (capability flags `canGetAndSetFMUstate` and `canSerializeFMUstate`). Typical uses are step rejection in a master algorithm, repeating a simulation from an intermediate point and saving a simulation to disk.

A snapshot contains everything that changes during a simulation: all variables (parameters, inputs, outputs, local variables, states), the model's unpublished internals, the initial guesses of the states that every initialization starts from, the internal clock, the experiment settings and the state of the FMU state machine. Instance name, callbacks and log level belong to the instance and are not part of it.

### fmi2GetFMUstate

Takes a snapshot of the FMU.

#### Signature
```c
fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate);
```

#### Description
Copies the current state of the FMU into a snapshot. If `*FMUstate` is NULL a new snapshot is allocated and returned in `*FMUstate`. If it points to a snapshot obtained earlier, that snapshot is overwritten and no memory is allocated.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `FMUstate` | `fmi2FMUstate*` | **In/out**: NULL to create a snapshot, or an existing snapshot to overwrite |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Snapshot taken |
| `fmi2Error` | NULL component, NULL `FMUstate`, `*FMUstate` is not a snapshot of this model, allocation failure, or FMU is in the Error state |

#### FMI State Requirements
**Can be called in**: Any state except **Error**
**State transitions**: None on success. On error the FMU enters the error state

#### Usage Example
```c
fmi2FMUstate snapshot = NULL;  // must be initialized with NULL

fmi2GetFMUstate(fmu, &snapshot);

// try a large step
if (fmi2DoStep(fmu, time, 0.1, fmi2False) != fmi2OK) {
    // go back and take two smaller ones
    fmi2SetFMUstate(fmu, snapshot);
    fmi2DoStep(fmu, time, 0.05, fmi2False);
    fmi2DoStep(fmu, time + 0.05, 0.05, fmi2False);
}

fmi2FreeFMUstate(fmu, &snapshot);
```

#### Related Functions
- [`fmi2SetFMUstate`](#fmi2setfmustate) - Return to the snapshot
- [`fmi2FreeFMUstate`](#fmi2freefmustate) - Free the snapshot

#### Notes
- **The snapshot belongs to the caller**: It is not freed by `fmi2FreeInstance`. Free it with `fmi2FreeFMUstate`
- **Initialize the pointer with NULL**: An uninitialized `fmi2FMUstate` would be taken for an existing snapshot
- **One block of memory**: A snapshot is a single allocation without pointers. All snapshots of a model have the same size, which is why an existing one can be reused
- **Contents**: the experiment (start and stop time), the state machine and all model data: the model's time, relative tolerance, parameters, inputs, outputs, states and state derivatives

---

### fmi2SetFMUstate

Sets the FMU back to a snapshot.

#### Signature
```c
fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate FMUstate);
```

#### Description
Restores all variables, the internal clock, the experiment settings and the state of the FMU state machine from a snapshot. The snapshot itself is not changed and can be used again.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `FMUstate` | `fmi2FMUstate` | Snapshot from `fmi2GetFMUstate` or `fmi2DeSerializeFMUstate` |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | FMU is back at the snapshot |
| `fmi2Error` | NULL component, or `FMUstate` is NULL or not a snapshot of this model |

#### FMI State Requirements
**Can be called in**: Any state, including **Error**
**State transitions**: To the state the FMU was in when the snapshot was taken

#### Usage Example
See [`fmi2GetFMUstate`](#fmi2getfmustate).

#### Related Functions
- [`fmi2GetFMUstate`](#fmi2getfmustate) - Take the snapshot
- [`fmi2Reset`](#fmi2reset) - The other way out of the Error state

#### Notes
- **Recovery from errors**: Because a snapshot can only be taken from a healthy FMU, restoring one always leaves the Error state
- **Recovery from a discarded step**: Restoring a snapshot taken before the failed step leaves the **Step Failed** state (see [Discarded Steps](#discarded-steps))
- **The next `fmi2DoStep` must start at the restored time**: The internal clock goes back with the snapshot
- **Works across instances**: A snapshot can be set into another instance of the same model

---

### fmi2FreeFMUstate

Frees a snapshot.

#### Signature
```c
fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate);
```

#### Description
Frees the memory of a snapshot and sets `*FMUstate` to NULL. A NULL pointer or a NULL snapshot is ignored.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `FMUstate` | `fmi2FMUstate*` | **In/out**: snapshot to free; set to NULL |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Snapshot freed (or nothing to free) |
| `fmi2Error` | NULL component |

#### FMI State Requirements
**Can be called in**: Any state, including **Error**
**State transitions**: None

#### Usage Example
```c
fmi2FreeFMUstate(fmu, &snapshot);  // snapshot is NULL afterwards
fmi2FreeFMUstate(fmu, &snapshot);  // harmless
```

#### Related Functions
- [`fmi2GetFMUstate`](#fmi2getfmustate) - Creates the snapshot

#### Notes
- **Free snapshots before the instance**: The function needs a valid component

---

### fmi2SerializedFMUstateSize / fmi2SerializeFMUstate / fmi2DeSerializeFMUstate

Convert a snapshot to a byte vector and back.

#### Signature
```c
fmi2Status fmi2SerializedFMUstateSize(fmi2Component c, fmi2FMUstate FMUstate, size_t* size);

fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate FMUstate,
                                 fmi2Byte serializedState[], size_t size);

fmi2Status fmi2DeSerializeFMUstate(fmi2Component c, const fmi2Byte serializedState[],
                                   size_t size, fmi2FMUstate* FMUstate);
```

#### Description
`fmi2SerializedFMUstateSize` returns the number of bytes needed, `fmi2SerializeFMUstate` copies the snapshot into a byte vector provided by the caller, and `fmi2DeSerializeFMUstate` creates a snapshot from such a byte vector. The bytes can be stored in a file and loaded in a later run.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `FMUstate` | `fmi2FMUstate` / `fmi2FMUstate*` | Snapshot to serialize, or **in/out** for deserialization (NULL to create, existing snapshot to overwrite) |
| `serializedState` | `fmi2Byte[]` | Byte vector, allocated by the caller |
| `size` | `size_t` / `size_t*` | Length of the byte vector, or **output** of `fmi2SerializedFMUstateSize` |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Success |
| `fmi2Error` | NULL component or pointer, invalid snapshot, byte vector too small (serialize), or wrong size / bytes that are not an FMU state of this model (deserialize) |

#### FMI State Requirements
**Can be called in**: Any state, including **Error** (the functions only handle snapshots)
**State transitions**: None on success. On error the FMU enters the error state

#### Usage Example
```c
fmi2FMUstate snapshot = NULL;
fmi2GetFMUstate(fmu, &snapshot);

// serialize
size_t size;
fmi2SerializedFMUstateSize(fmu, snapshot, &size);
fmi2Byte* bytes = malloc(size);
fmi2SerializeFMUstate(fmu, snapshot, bytes, size);
fmi2FreeFMUstate(fmu, &snapshot);

// ... write bytes to a file, read them back later ...

// deserialize and continue from there
fmi2FMUstate restored = NULL;
if (fmi2DeSerializeFMUstate(fmu, bytes, size, &restored) == fmi2OK) {
    fmi2SetFMUstate(fmu, restored);
    fmi2FreeFMUstate(fmu, &restored);
}
free(bytes);
```

#### Related Functions
- [`fmi2GetFMUstate`](#fmi2getfmustate) / [`fmi2SetFMUstate`](#fmi2setfmustate) - Take and restore snapshots

#### Notes
- **The serialized form is a copy of the snapshot's memory**: This is possible because a snapshot contains no pointers. The bytes can only be read back by the same FMU binary: not on another platform and not after the model's structs have changed
- **Bytes from outside are verified**: Size, a marker, a hash of the model's GUID and the stored state are checked before the bytes are accepted. This catches states of another model and corrupted headers, but not manipulated variable values
- **Deterministic**: Two snapshots of the same FMU state serialize to the same bytes

---

## Shutdown

Functions for gracefully ending simulation and cleaning up FMU resources.

### fmi2Terminate

Signals the end of simulation and prepares the FMU for cleanup.

#### Signature
```c
fmi2Status fmi2Terminate(fmi2Component c);
```

#### Description
Signals that the simulation has ended and the FMU should perform any necessary finalization before destruction. This function represents **logical shutdown** - closing files, finalizing computations, releasing simulation-specific resources - as distinct from `fmi2FreeInstance` which handles **physical cleanup** (memory deallocation). After calling this function, the FMU transitions to the Terminated state where only `fmi2FreeInstance` or `fmi2Reset` can be called.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance to terminate |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Termination successful, FMU is ready for cleanup |
| `fmi2Error` | NULL component or FMU not in a valid state for termination |

#### FMI State Requirements
**Can be called in**: **Step Complete** or **Step Failed** (after simulation ends)
**State transitions**: Transitions from current state to **Terminated** state

#### Valid Call Sequence
The complete co-simulation lifecycle is:
```c
fmi2Instantiate       → Instantiated
fmi2SetupExperiment   → Instantiated
fmi2EnterInitMode     → Initialization Mode
fmi2ExitInitMode      → Step Complete
fmi2DoStep (loop)     → Step Complete
fmi2Terminate         → Terminated  ← This function
fmi2FreeInstance      → (destroyed)
```

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);
fmi2EnterInitializationMode(fmu);
fmi2ExitInitializationMode(fmu);

// Simulation loop
fmi2ValueReference inputRef = 2, outputRef = 3;
fmi2Real input = 1.0, output;
double t = 0.0, dt = 0.01, stopTime = 10.0;

while (t < stopTime) {
    fmi2SetReal(fmu, &inputRef, 1, &input);
    fmi2DoStep(fmu, t, dt, fmi2True);
    fmi2GetReal(fmu, &outputRef, 1, &output);
    printf("t=%.2f, y=%.4f\n", t, output);
    t += dt;
}

// Graceful shutdown
fmi2Status status = fmi2Terminate(fmu);
if (status != fmi2OK) {
    fprintf(stderr, "Warning: Terminate failed\n");
}

// Final cleanup
fmi2FreeInstance(fmu);
```

#### Error Handling Example
```c
// Simulation with error handling
fmi2Component fmu = fmi2Instantiate(/* ... */);
// ... setup and initialization ...

double t = 0.0, dt = 0.01;
fmi2Status status = fmi2OK;

for (int i = 0; i < 1000; i++) {
    status = fmi2DoStep(fmu, t, dt, fmi2True);
    if (status >= fmi2Error) {
        fprintf(stderr, "Step failed at t=%.2f\n", t);
        break;
    }
    t += dt;
}

// After an error the FMU can only be reset or freed: do not terminate
if (status < fmi2Error) {
    fmi2Terminate(fmu);
}
fmi2FreeInstance(fmu);
```

#### Related Functions
- [`fmi2DoStep`](#fmi2dostep) - Last function called before termination
- [`fmi2FreeInstance`](#fmi2freeinstance) - Must be called after termination to free memory
- [`fmi2Reset`](#fmi2reset) - Alternative to FreeInstance for reusing the FMU

#### Notes
- **Two-phase destruction pattern**: `fmi2Terminate` handles logical shutdown, `fmi2FreeInstance` handles memory cleanup
- **Not strictly required**: Integration tools can skip `fmi2Terminate` and call `fmi2FreeInstance` directly, but termination is best practice
- **Template implementation is minimal**: This FMU template has no files or resources to close, so the function just transitions state
- **Real FMUs might**: Close log files, disconnect from hardware, finalize statistics, save checkpoint data
- **After termination**: Only `fmi2FreeInstance` and `fmi2Reset` are allowed - prevents accidental re-simulation
- **Not callable in the Error state**: After a function returned `fmi2Error`, `fmi2Terminate` fails as well. Free or reset the FMU instead
- **Callable after a discarded step**: In the **Step Failed** state (after `fmi2DoStep` returned `fmi2Discard`) the simulation can be ended with `fmi2Terminate`
- **Event logging**: Logs "FMU was terminated" when logging is enabled, useful for debugging lifecycle issues
- **Idiomatic C pattern**: Two-phase destruction (logical shutdown + memory cleanup) appears in SDL, OpenGL, database libraries

#### Why Separate Terminate and FreeInstance?

**Scenario: Complex FMU with resources**
```c
// Hypothetical FMU that logs to file
fmi2DoStep(...);  // Last simulation step
fmi2Terminate(fmu);  // Writes final statistics, closes log file gracefully
fmi2FreeInstance(fmu);  // Frees memory
```

**Benefits:**
1. **Graceful resource cleanup**: Files closed properly, buffers flushed
2. **Separation of concerns**: Simulation logic vs. memory management
3. **Debugging**: Terminate can log final state for diagnostics

**In this template**: The function is simple (just state transition) because the template has no resources, but the pattern is established for future extensions.

---

### fmi2Reset

Resets the FMU to the state immediately after `fmi2Instantiate`.

#### Signature
```c
fmi2Status fmi2Reset(fmi2Component c);
```

#### Description
Resets the FMU back to its post-instantiation state without freeing and reallocating memory. The simulation time is cleared and all model variables are set back to their start values, while the instance identity (name, callbacks, log level) is preserved. After a successful reset, the caller must repeat the full initialization cycle (`fmi2SetupExperiment` → `fmi2EnterInitializationMode` → `fmi2ExitInitializationMode`) before stepping again.

This function is the preferred alternative to destroying and recreating the FMU when running repeated simulations (e.g., parameter sweeps or optimization loops), since it avoids the overhead of memory reallocation.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance to reset |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Reset successful, FMU is back in Instantiated state |
| `fmi2Error` | NULL component |

#### FMI State Requirements
**Can be called in**: Any state after instantiation, including the **Error** state
**State transitions**: Transitions from current state to **Instantiated**

#### What Gets Reset
| Category | Behavior |
|----------|----------|
| Experiment | `startTime`, `stopTime`, `time` → `0.0`, no stop time defined |
| Parameters, inputs, states | Back to the start values of the model's variable table |
| Outputs | All zeroed and marked as stale (computed again when read and in `fmi2ExitInitializationMode`) |
| State derivatives | All zeroed |
| Function pointers | Re-bound from the model definition |
| FMU state machine | → `stateInstantiated` |

#### What Is Preserved
| Category | Behavior |
|----------|----------|
| Instance name | Unchanged |
| Logger callback and component environment | Unchanged |
| Log level | Unchanged |

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);

// First simulation run
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 1.0);
fmi2EnterInitializationMode(fmu);
fmi2ExitInitializationMode(fmu);
for (double t = 0.0; t < 1.0; t += 0.01) {
    fmi2DoStep(fmu, t, 0.01, fmi2True);
}
fmi2Terminate(fmu);

// Reset instead of free + re-instantiate
fmi2Reset(fmu);

// Second simulation run with different parameters
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 2.0);
fmi2EnterInitializationMode(fmu);
// Override a parameter before exiting init mode
fmi2ValueReference paramRef = 0;  // tau (first_order)
fmi2Real newTau = 0.5;
fmi2SetReal(fmu, &paramRef, 1, &newTau);
fmi2ExitInitializationMode(fmu);
for (double t = 0.0; t < 2.0; t += 0.01) {
    fmi2DoStep(fmu, t, 0.01, fmi2True);
}
fmi2Terminate(fmu);

// Final cleanup
fmi2FreeInstance(fmu);
```

#### Parameter Sweep Example
```c
// Run multiple simulations with different gain values
fmi2Component fmu = fmi2Instantiate(/* ... */);
fmi2ValueReference gainRef = 1;  // k parameter
fmi2ValueReference outputRef = 3;
double gains[] = {0.5, 1.0, 2.0, 5.0};

for (int i = 0; i < 4; i++) {
    fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 1.0);
    fmi2EnterInitializationMode(fmu);
    fmi2SetReal(fmu, &gainRef, 1, &gains[i]);
    fmi2ExitInitializationMode(fmu);

    for (double t = 0.0; t < 1.0; t += 0.01) {
        fmi2DoStep(fmu, t, 0.01, fmi2True);
    }

    fmi2Real finalOutput;
    fmi2GetReal(fmu, &outputRef, 1, &finalOutput);
    printf("k=%.1f → y_final=%.4f\n", gains[i], finalOutput);

    fmi2Terminate(fmu);
    if (i < 3) fmi2Reset(fmu);  // Reset for next run (skip on last)
}

fmi2FreeInstance(fmu);
```

#### Related Functions
- [`fmi2Terminate`](#fmi2terminate) - Typically called before Reset to end the current simulation
- [`fmi2FreeInstance`](#fmi2freeinstance) - Alternative to Reset when the FMU is no longer needed
- [`fmi2Instantiate`](#fmi2instantiate) - Reset returns to the state produced by this function
- [`fmi2EnterInitializationMode`](#fmi2enterinitializationmode) - Must be called after Reset to re-initialize

#### Notes
- **Soft re-instantiation**: Avoids the cost of `fmi2FreeInstance` + `fmi2Instantiate` by reusing existing memory allocations
- **Same state as after instantiation**: Reset and `fmi2Instantiate` share one function (`resetModel()`), so the variables are guaranteed to be identical in both cases. Values set before the reset are discarded
- **Stepping still requires initialization**: `fmi2DoStep` on a reset-but-uninitialized FMU is rejected by the state machine
- **Error state recovery**: Reset is the way out of the Error state without destroying the instance (the other one is `fmi2SetFMUstate`)
- **Snapshots survive**: FMU states taken with `fmi2GetFMUstate` stay valid across a reset
- **Log level preserved**: The logging configuration survives a reset, which is useful for debugging repeated runs

---

## Unsupported Functions

The following functions belong to capabilities this FMU does not have. The capability flags in the `modelDescription.xml` tell the integration tool not to call them. If they are called anyway, they return `fmi2Error` and the FMU enters the Error state.

| Function | Capability | Flag in `modelDescription.xml` |
|----------|------------|--------------------------------|
| `fmi2CancelStep` | Asynchronous steps (`fmi2DoStep` never returns `fmi2Pending`) | - |
| `fmi2SetRealInputDerivatives` | Input interpolation | `canInterpolateInputs="false"` |
| `fmi2GetRealOutputDerivatives` | Output derivatives | `maxOutputDerivativeOrder="0"` |
| `fmi2GetDirectionalDerivative` | Directional derivatives | `providesDirectionalDerivative="false"` |

The Model Exchange functions (`fmi2SetTime`, `fmi2SetContinuousStates`, `fmi2GetDerivatives`, ...) are exported because the standard header declares them. They return `fmi2Error` without changing the FMU.

---
