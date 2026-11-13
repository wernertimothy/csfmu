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
- [Shutdown](#shutdown)
  - [fmi2Terminate](#fmi2terminate)
  - [fmi2Reset](#fmi2reset)

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
| `fmuType` | `fmi2Type` | Type of FMU: `fmi2CoSimulation` or `fmi2ModelExchange` |
| `fmuGUID` | `fmi2String` | GUID from modelDescription.xml - must match FMU's GUID |
| `fmuResourceLocation` | `fmi2String` | URI to the `resources` directory (may be used for loading data files) |
| `functions` | `const fmi2CallbackFunctions*` | Pointer to callback function structure (logger, allocator, etc.) |
| `visible` | `fmi2Boolean` | Whether simulation should be visible (UI-related, often ignored) |
| `loggingOn` | `fmi2Boolean` | Whether logging should be enabled initially |

#### Return Values
| Return Value | Condition |
|--------------|-----------|
| Valid `fmi2Component` pointer | Successfully instantiated |
| `NULL` | Instantiation failed (GUID mismatch, allocation failure, invalid callbacks) |

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
- All callback functions in `fmi2CallbackFunctions` must be valid (non-NULL), especially `logger`, `allocateMemory`, `freeMemory`, and `componentEnvironment`
- Internally allocates both `myFMU2` wrapper and `modelInstance` using `newModel(&MODEL_DEFINITION)`
- **All variables hold their start values** after instantiation (parameters, inputs and states as declared in the model's variable table). Values set afterwards with `fmi2SetReal` etc. are kept through initialization
- Returns an opaque `fmi2Component` pointer - integration tool should never access internals directly
- If instantiation fails, logs error via the logger callback before returning NULL

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
| `tolerance` | `fmi2Real` | Relative tolerance for numerical integration (often ignored in co-simulation) |
| `startTime` | `fmi2Real` | Start time of the experiment in seconds |
| `stopTimeDefined` | `fmi2Boolean` | Whether `stopTime` parameter is meaningful |
| `stopTime` | `fmi2Real` | Stop time of the experiment in seconds |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Experiment setup successful |
| `fmi2Error` | Invalid parameters or wrong state |

#### FMI State Requirements
**Can be called in**: **Instantiated** state (after `fmi2Instantiate`, before `fmi2EnterInitializationMode`)
**State transitions**: Remains in **Instantiated** state

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);

// Setup a simulation from t=0 to t=10 seconds
fmi2Status status = fmi2SetupExperiment(
    fmu,
    fmi2False,  // tolerance not defined (common for co-sim)
    0.0,        // tolerance (ignored)
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
- `tolerance` is typically undefined for co-simulation (solver-specific)
- Internally stores `startTime`, `stopTime`, and initializes internal time clock to `startTime`
- Currently validates that `startTime < stopTime` and returns error if violated
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
| `fmi2Error` | Invalid parameters (NULL component or NULL categories with nCategories > 0) |

#### FMI State Requirements
**Can be called in**: Any state after instantiation (very flexible)
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
   - Unknown category names are silently ignored (best effort)

#### Log Categories

This FMU supports the following standard FMI 2.0 categories (from spec section 2.2.4):

**Supported:**
- `logAll` - Enable all categories
- `logEvents` - Event detection and handling
- `logStatusWarning` - Warning messages
- `logStatusError` - Error messages (always enabled by default)

**Defined but not supported** (silently ignored):
- `logSingularLinearSystems`
- `logNonlinearSystems`
- `logDynamicStateSelection`
- `logStatusDiscard`
- `logStatusFatal`
- `logStatusPending`

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);

// Example 1: Enable only error logging (default)
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
- **Helper function available**: Internal `logCategoryActive(fmu, category)` checks if a category is enabled
- **Thread-safe**: Can be called from any thread (though the logger callback itself must be thread-safe)
- **Dynamic control**: Integration tools can enable verbose logging only during problematic simulation periods
- **Categories parameter can be NULL**: When `nCategories == 0`, NULL is valid and means "enable all"
- **Unknown categories ignored**: Per FMI spec, best effort - unrecognized category names don't cause errors
- **Default logging**: Errors are logged by default even if no categories explicitly enabled

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
Moves the FMU from the instantiated state into initialization mode. The function does not change any variable: all variables received their start values in `fmi2Instantiate` (or `fmi2Reset`), and values the integration tool has set since then are kept. The integration tool can continue to set parameters, inputs and states by calling `fmi2SetReal`, `fmi2SetInteger`, etc. before calling `fmi2ExitInitializationMode` to finalize initialization.

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
**State transitions**: Adds **Initialization Mode** state flag (FMU remains instantiated)

#### Where Initial Values Come From

| Step | What happens to the variables |
|------|-------------------------------|
| `fmi2Instantiate` / `fmi2Reset` | Every variable is set to the start value declared in the model's variable table |
| Instantiated state | Integration tool may set parameters, inputs and states |
| `fmi2EnterInitializationMode` | Nothing (values are kept) |
| Initialization mode | Integration tool may set parameters, inputs and states |
| `fmi2ExitInitializationMode` | Outputs are computed from the initial values |

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

---

### fmi2ExitInitializationMode

Finalizes initialization and transitions to step mode, ready for simulation.

#### Signature
```c
fmi2Status fmi2ExitInitializationMode(fmi2Component c);
```

#### Description
Completes the initialization phase and transitions the FMU from initialization mode to initialized/step mode. The outputs are computed from the initial states, inputs and parameters, so they are valid at the first communication point. After this function returns successfully, the FMU is ready to execute simulation steps via `fmi2DoStep`. This is the final step before beginning the simulation loop. No more initialization-time variable setting is allowed after this call.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Successfully exited initialization mode and ready for stepping |
| `fmi2Error` | NULL component or FMU not in initialization mode |

#### FMI State Requirements
**Can be called in**: **Initialization Mode** (after `fmi2EnterInitializationMode`)
**State transitions**: Removes **Initialization Mode** flag, adds **Initialized** state flag

#### State Transition Details

The function performs a two-step state transition:

1. **Remove initialization mode**: `state &= ~stateInitializationMode` clears the initialization flag
2. **Add initialized state**: `state |= stateInitialized` sets the initialized flag
3. **Preserve instantiated state**: The FMU remains instantiated (bitmask allows multiple states)

After this call, the FMU state is: `stateInstantiated | stateInitialized`

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
- **States and fixed parameters are frozen**: After this call, states and parameters declared as `VAR_FIXED` can no longer be set
- **No parameter validation**: The FMU trusts that the integration tool has set all required values during initialization mode
- **Finalization point**: After this call, `fmi2EnterInitializationMode` cannot be called again without calling `fmi2Reset` first
- **Ready for stepping**: Integration tool typically reads initial outputs immediately after this call, then begins the simulation loop
- **Bitwise state machine**: Uses `&= ~flag` to remove a flag and `|= flag` to add a flag - allows multiple states to coexist
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
| Output | Never |

Integration tools should always look up value references by variable name in the `modelDescription.xml` and never hardcode them.

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
| `fmi2Error` | NULL component, call not allowed in the current state, invalid arrays, unknown value reference, variable is not of type Real, variable is an output, or variable is a state or fixed parameter and the initialization has finished |

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
Reads one or more real-valued variables by their value references. Every real variable of the model can be read. Integration tools typically use this after `fmi2DoStep` to read output values.

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
| `fmi2Error` | NULL component, call not allowed in the current state, invalid arrays, unknown value reference, or variable is not of type Real |

#### FMI State Requirements
**Can be called in**: **Instantiated**, **Step Complete**, **Step Failed**, **Step Canceled**, **Terminated**, **Error**
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
- **Outputs are valid from the end of initialization**: They are computed in `fmi2ExitInitializationMode` and after every `fmi2DoStep`
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
The integer counterparts of `fmi2SetReal` and `fmi2GetReal`. A variable is an integer variable if its field in the model's struct is of type `int`.

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
| `fmi2Error` | Same conditions as `fmi2SetReal` / `fmi2GetReal`; the variable must be of type Integer |

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
- **Discrete in the model description**: Integer inputs and outputs are declared with `variability="discrete"`, as required by FMI 2.0

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

String variables are not supported.

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
The template does not support string variables, so no value reference is valid for these functions. They exist because the FMI 2.0 standard requires every FMU to export them.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance |
| `vr` | `const fmi2ValueReference[]` | Array of value references |
| `nvr` | `size_t` | Number of value references |
| `value` | `fmi2String[]` | Values |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | `nvr` is 0 |
| `fmi2Error` | `nvr` is greater than 0, NULL component, call not allowed in the current state, or invalid arrays |

#### FMI State Requirements
**Can be called in**: Same states as [`fmi2SetReal`](#fmi2setreal) and [`fmi2GetReal`](#fmi2getreal)
**State transitions**: None on success. On error the FMU enters the error state

#### Usage Example
```c
// There are no string variables: only the empty call succeeds
fmi2Status status = fmi2GetString(fmu, NULL, 0, NULL);  // fmi2OK
```

#### Related Functions
- [`fmi2SetReal`](#fmi2setreal) / [`fmi2GetReal`](#fmi2getreal) - Access to real variables

#### Notes
- **Fails loudly**: A call with a value reference returns `fmi2Error` instead of reporting success without doing anything

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
| `currentCommunicationPoint` | `fmi2Real` | The current simulation time (should match FMU's internal clock) |
| `communicationStepSize` | `fmi2Real` | Time increment to advance (must be > 0) |
| `noSetFMUStatePriorToCurrentPoint` | `fmi2Boolean` | Flag for FMU state management (ignored in this implementation) |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Step completed successfully |
| `fmi2Error` | Invalid component, negative/zero step size, wrong state, or time inconsistency |

#### FMI State Requirements
**Can be called in**: **Initialized** state (after `fmi2ExitInitializationMode`, before `fmi2Terminate`)
**State transitions**: Remains in **Initialized** state

#### Algorithm: Sub-Stepping for Numerical Stability

The implementation uses **adaptive sub-stepping** to maintain numerical accuracy:

1. **Calculate sub-steps**:
   ```c
   numberOfSteps = floor(communicationStepSize / maxStepSize)
   stepRemainder = communicationStepSize - (numberOfSteps * maxStepSize)
   ```

2. **Execute full steps**:
   - Take `numberOfSteps` steps of size `maxStepSize`
   - Each step: compute derivatives → integrate states

3. **Handle remainder**:
   - If `stepRemainder >= EPSILON` (1e-9 s): take final step of size `stepRemainder`
   - If `stepRemainder < EPSILON`: skip (numerically insignificant)

4. **Update time and outputs**:
   - Advance internal clock by `communicationStepSize`
   - Compute outputs from final state

**Why sub-stepping?** Explicit integrators like Euler Forward have stability limits. For typical first-order systems, `maxStepSize = 1e-3` ensures stable integration even when the integration tool requests larger communication steps (e.g., 0.1 s).

#### Time Consistency Validation

Before stepping, validates that `currentCommunicationPoint` matches the FMU's internal clock (within tolerance `EPSILON = 1e-9`). This catches:
- Integration tool errors (wrong time tracking)
- FMU implementation bugs
- Co-simulation orchestration issues

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
1. Calculate: `numberOfSteps = 2`, `stepRemainder = 0.005 s`
2. Step 1: Integrate 0.01 s (states: t → t+0.01)
3. Step 2: Integrate 0.01 s (states: t+0.01 → t+0.02)
4. Remainder: Integrate 0.005 s (states: t+0.02 → t+0.025)
5. Update clock: `time += 0.025 s`
6. Compute outputs at t+0.025

**Result**: FMU advanced full 0.025 s using three stable sub-steps.

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
- **EPSILON = 1e-9 s**: Threshold for "numerically insignificant" - balances precision vs performance
- **Integer loop counter**: Avoids floating-point accumulation errors in sub-step counting
- **Time consistency enforced**: Returns error if `currentCommunicationPoint` doesn't match internal clock
- **Synchronous only**: Blocks until computation completes (`noSetFMUStatePriorToCurrentPoint` ignored)
- **No variable step size within DoStep**: Uses fixed sub-steps (adaptive methods would require more complex integrators)

#### Configuration

Adjust in the model definition (`src/models/<model_name>/variables.c`):
```c
    .maxStepSize = 1e-3,  // Maximum stable integration step
```

Smaller values → more stable but slower
Larger values → faster but may diverge for stiff systems

#### Advanced: Epsilon Choice

`EPSILON = 1e-9` (1 nanosecond) chosen because:
- **Physical insignificance**: Below classical physics regime
- **Numerical robustness**: 10⁶× larger than machine epsilon (2.2e-16)
- **Accumulated error safety**: Provides margin for floating-point drift
- **Industry standard**: Matches MATLAB, SciPy, FMI examples

Remainders < 1e-9 s are skipped as numerical noise.

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

#### Error Recovery Example
```c
// Simulation with error handling
fmi2Component fmu = fmi2Instantiate(/* ... */);
// ... setup and initialization ...

double t = 0.0, dt = 0.01;
fmi2Status status;

for (int i = 0; i < 1000; i++) {
    status = fmi2DoStep(fmu, t, dt, fmi2True);
    if (status >= fmi2Error) {
        // Step failed - still terminate gracefully
        fprintf(stderr, "Step failed at t=%.2f\n", t);
        break;
    }
    t += dt;
}

// Terminate works even after DoStep failure
fmi2Terminate(fmu);  // Allowed from stateStepFailed
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
- **Callable after failure**: Can be called from `stateStepFailed` to enable graceful shutdown even when simulation encounters errors
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
2. **Error recovery**: Can terminate even after failures
3. **Separation of concerns**: Simulation logic vs. memory management
4. **Debugging**: Terminate can log final state for diagnostics

**In this template**: The function is simple (just state transition) because the template has no resources, but the pattern is established for future extensions.

---

### fmi2Reset

Resets the FMU to the state immediately after `fmi2Instantiate`.

#### Signature
```c
fmi2Status fmi2Reset(fmi2Component c);
```

#### Description
Resets the FMU back to its post-instantiation state without freeing and reallocating memory. The simulation time is cleared and all model variables are set back to their start values, while the instance identity (name, callbacks, FMU type, log level) is preserved. After a successful reset, the caller must repeat the full initialization cycle (`fmi2SetupExperiment` → `fmi2EnterInitializationMode` → `fmi2ExitInitializationMode`) before stepping again.

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
**Can be called in**: Any state except **Start/End** (pseudo-state before instantiation)
**State transitions**: Transitions from current state to **Instantiated**

#### What Gets Reset
| Category | Behavior |
|----------|----------|
| Simulation time | `startTime`, `stopTime`, `time` → `0.0` |
| Parameters, inputs, states | Back to the start values of the model's variable table |
| Outputs | All zeroed (computed again in `fmi2ExitInitializationMode`) |
| State derivatives | All zeroed |
| Function pointers | Re-bound from the model definition |
| FMU state machine | → `stateInstantiated` |

#### What Is Preserved
| Category | Behavior |
|----------|----------|
| Instance name | Unchanged |
| FMU type | Unchanged |
| Callbacks (logger, allocate, free) | Unchanged |
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
- **Error state recovery**: Reset can be called from error states, providing a way to recover without destroying the instance
- **Log level preserved**: The logging configuration survives a reset, which is useful for debugging repeated runs

---