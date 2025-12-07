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
  - [fmi2SetInteger](#fmi2setinteger)
  - [fmi2GetInteger](#fmi2getinteger)
  - [fmi2SetBoolean](#fmi2setboolean)
  - [fmi2GetBoolean](#fmi2getboolean)
  - [fmi2SetString](#fmi2setstring)
  - [fmi2GetString](#fmi2getstring)

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
- **GUID validation is mandatory** - the provided `fmuGUID` must exactly match the GUID in `config.h` (currently `{a1b2c3d4-e5f6-7890-abcd-ef1234567890}`)
- All callback functions in `fmi2CallbackFunctions` must be valid (non-NULL), especially `logger`, `allocateMemory`, `freeMemory`, and `componentEnvironment`
- Internally allocates both `myFMU2` wrapper and `modelInstance` using `newModel(NUMBER_OF_STATES, NUMBER_OF_INPUTS)`
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
Moves the FMU from the instantiated state into initialization mode. This function sets default initial values for states and parameters from the configuration, and assigns the state equation and integrator function pointers. After calling this function, the integration tool can override default values by calling `fmi2SetReal`, `fmi2SetInteger`, etc. before calling `fmi2ExitInitializationMode` to finalize initialization.

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

#### Internal Initialization

When this function is called, the following initializations occur:

1. **States**: Initialized to `INITIAL_STATE` from `config.h` (default: `{0.0}`)
2. **Parameters**: Initialized to `INITIAL_PARAMS` from `config.h` (default: `{0.15, 1.0}` for tau and k)
3. **State equation**: Function pointer set to `MODEL_STATE_EQUATION` (default: `firstOrderSystem`)
4. **Integrator**: Function pointer set to `INTEGRATOR_METHOD` (default: `eulerForward`)
5. **Inputs**: Already zero-initialized by `calloc` in model creation
6. **State derivatives**: Already zero-initialized, will be computed on first `fmi2DoStep`

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);

// Setup the experiment
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);

// Enter initialization mode (sets defaults)
fmi2Status status = fmi2EnterInitializationMode(fmu);
if (status != fmi2OK) {
    fprintf(stderr, "Failed to enter initialization mode\n");
    return -1;
}

// Override default initial state (optional)
fmi2Real initialState = 5.0;
fmi2ValueReference stateRef = 0;
fmi2SetReal(fmu, &stateRef, 1, &initialState);

// Override default parameters (optional)
fmi2Real tau = 0.2, k = 2.0;
fmi2ValueReference paramRefs[] = {100, 101};  // Value references for parameters
fmi2Real paramValues[] = {tau, k};
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
- **Two-phase initialization**: This function sets defaults, integration tool can override them, then `fmi2ExitInitializationMode` finalizes
- **State machine uses bitmasks**: The FMU remains in instantiated state while also being in initialization mode - flags are additive
- **Defaults from config**: Initial states come from `INITIAL_STATE[]` array in `config.h`, parameters from `INITIAL_PARAMS[]`
- **Function pointers enable flexibility**: Different models can use different integrators and state equations via configuration macros
- **Arrays pre-zeroed**: Inputs and state derivatives were already zero-initialized by `calloc` in `newModel()` - no action needed here
- **Must be instantiated first**: Returns `fmi2Error` if called before `fmi2Instantiate` or `fmi2SetupExperiment`
- **Parameter structure**: Uses flexible `double* parameter` array rather than fixed struct - allows arbitrary parameter dimensions

---

### fmi2ExitInitializationMode

Finalizes initialization and transitions to step mode, ready for simulation.

#### Signature
```c
fmi2Status fmi2ExitInitializationMode(fmi2Component c);
```

#### Description
Completes the initialization phase and transitions the FMU from initialization mode to initialized/step mode. After this function returns successfully, the FMU is ready to execute simulation steps via `fmi2DoStep`. This is the final step before beginning the simulation loop. No more initialization-time variable setting is allowed after this call.

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
- **Simple for co-simulation**: This function is straightforward for co-simulation FMUs - just a state transition. Model exchange FMUs would need to solve initial equations here.
- **No parameter validation**: The FMU trusts that the integration tool has set all required values during initialization mode
- **Finalization point**: After this call, `fmi2EnterInitializationMode` cannot be called again without calling `fmi2Reset` first
- **Ready for stepping**: Integration tool typically reads initial outputs immediately after this call, then begins the simulation loop
- **Bitwise state machine**: Uses `&= ~flag` to remove a flag and `|= flag` to add a flag - allows multiple states to coexist
- **Error if wrong state**: Returns `fmi2Error` if not currently in initialization mode (catches lifecycle violations)

---

## Variable Access

Functions for setting and reading variable values during initialization and simulation.

### fmi2SetReal

Sets real-valued variables (parameters, inputs).

#### Signature
```c
fmi2Status fmi2SetReal(fmi2Component c,
                       const fmi2ValueReference vr[],
                       size_t nvr,
                       const fmi2Real value[]);
```

#### Description
Sets one or more real-valued variables by their value references. Parameters can only be set during initialization mode, while inputs can be set during initialization or step mode. This is the primary mechanism for the integration tool to provide input signals and configure parameters.

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
| `fmi2Error` | NULL component, invalid arrays, invalid VR, or attempting to set parameter outside initialization mode |

#### FMI State Requirements
**Can be called in**: **Initialization Mode** or **Step Mode** (after initialization)
**State transitions**: None (setter only)

#### Value Reference Mapping

This FMU uses sequential value reference ranges:

| VR Range | Variable Type | Settable When | Notes |
|----------|---------------|---------------|-------|
| 0 to `NUMBER_OF_PARAMS-1` | Parameters | Initialization mode only | Returns error if set after initialization |
| `NUMBER_OF_PARAMS` to `NUMBER_OF_PARAMS+NUMBER_OF_INPUTS-1` | Inputs | Initialization or step mode | Can be updated before each `fmi2DoStep` |
| `NUMBER_OF_PARAMS+NUMBER_OF_INPUTS` onward | Outputs | **Never** (read-only) | Returns error if attempted |

**Example for default template** (2 params, 1 input, 1 output):
- VR 0: Parameter `tau` (settable during init only)
- VR 1: Parameter `k` (settable during init only)
- VR 2: Input `u` (settable anytime)
- VR 3: Output `y` (read-only, use `fmi2GetReal`)

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);
fmi2EnterInitializationMode(fmu);

// Set parameters during initialization
fmi2ValueReference paramRefs[] = {0, 1};  // tau, k
fmi2Real paramValues[] = {0.2, 2.5};
fmi2Status status = fmi2SetReal(fmu, paramRefs, 2, paramValues);
if (status != fmi2OK) {
    fprintf(stderr, "Failed to set parameters\n");
}

fmi2ExitInitializationMode(fmu);

// Set input during simulation
fmi2ValueReference inputRef = 2;  // u
fmi2Real inputValue = 1.0;
fmi2SetReal(fmu, &inputRef, 1, &inputValue);

// Attempt to set parameter after initialization (ERROR)
fmi2Real newTau = 0.3;
status = fmi2SetReal(fmu, &paramRefs[0], 1, &newTau);  // Returns fmi2Error
```

#### Related Functions
- [`fmi2GetReal`](#fmi2getreal) - Read real-valued variables
- [`fmi2EnterInitializationMode`](#fmi2enterinitializationmode) - Enables parameter setting
- [`fmi2DoStep`](#fmi2dostep) - Use inputs set via this function

#### Notes
- **Bulk operations**: Arrays allow setting multiple variables in one call for efficiency
- **Sequential VR mapping**: Value references directly map to array indices within each type range
- **Parameter immutability**: Parameters become read-only after exiting initialization mode (per FMI standard)
- **No state setting**: States are internal to the model and cannot be directly set via FMI API
- **nvr=0 is valid**: Calling with empty arrays is a no-op, not an error
- **Error handling**: Returns error on first invalid VR encountered (fail-fast pattern)

---

### fmi2GetReal

Reads real-valued variables (parameters, inputs, outputs).

#### Signature
```c
fmi2Status fmi2GetReal(fmi2Component c,
                       const fmi2ValueReference vr[],
                       size_t nvr,
                       fmi2Real value[]);
```

#### Description
Reads one or more real-valued variables by their value references. Can read parameters, inputs, and outputs. Integration tools typically use this after `fmi2DoStep` to read output values, or during initialization to verify parameter values.

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
| `fmi2Error` | NULL component, invalid arrays, or invalid VR |

#### FMI State Requirements
**Can be called in**: **Initialization Mode** or **Step Mode** (after initialization)
**State transitions**: None (getter only)

#### Value Reference Mapping

This FMU uses sequential value reference ranges:

| VR Range | Variable Type | Example (Default Template) |
|----------|---------------|----------------------------|
| 0 to `NUMBER_OF_PARAMS-1` | Parameters | VR 0: `tau`, VR 1: `k` |
| `NUMBER_OF_PARAMS` to `NUMBER_OF_PARAMS+NUMBER_OF_INPUTS-1` | Inputs | VR 2: `u` |
| `NUMBER_OF_PARAMS+NUMBER_OF_INPUTS` to `...+NUMBER_OF_OUTPUTS-1` | Outputs | VR 3: `y` |

**Output values**: Outputs are computed by the model (see `modelDoTimeStep()` in `model.c`) and stored in `model->outputs[]` array.

#### Usage Example
```c
fmi2Component fmu = fmi2Instantiate(/* ... */);
fmi2SetupExperiment(fmu, fmi2False, 0.0, 0.0, fmi2True, 10.0);
fmi2EnterInitializationMode(fmu);

// Read back parameters to verify
fmi2ValueReference paramRefs[] = {0, 1};
fmi2Real paramValues[2];
fmi2GetReal(fmu, paramRefs, 2, paramValues);
printf("tau=%.3f, k=%.3f\n", paramValues[0], paramValues[1]);

fmi2ExitInitializationMode(fmu);

// Simulation loop: set input, step, read output
fmi2ValueReference inputRef = 2;
fmi2ValueReference outputRef = 3;
fmi2Real input = 1.0, output;

for (double t = 0.0; t < 10.0; t += 0.01) {
    fmi2SetReal(fmu, &inputRef, 1, &input);
    fmi2DoStep(fmu, t, 0.01, fmi2True);
    fmi2GetReal(fmu, &outputRef, 1, &output);
    printf("t=%.2f, u=%.2f, y=%.4f\n", t, input, output);
}
```

#### Related Functions
- [`fmi2SetReal`](#fmi2setreal) - Set real-valued variables
- [`fmi2DoStep`](#fmi2dostep) - Updates outputs based on inputs

#### Notes
- **Output array is written**: The `value[]` parameter is an output, caller must allocate space for `nvr` elements
- **Outputs updated by model**: Outputs are computed during `fmi2DoStep` via `modelDoTimeStep()` function
- **Sequential VR mapping**: Value references map to array indices within each type range
- **Bulk operations**: Read multiple variables in one call for efficiency
- **nvr=0 is valid**: Calling with empty arrays is a no-op
- **All variable types readable**: Unlike `fmi2SetReal`, this can read outputs as well as parameters and inputs

---

### fmi2SetInteger

Sets integer-valued variables (not used in this template).

#### Signature
```c
fmi2Status fmi2SetInteger(fmi2Component c,
                          const fmi2ValueReference vr[],
                          size_t nvr,
                          const fmi2Integer value[]);
```

#### Description
Stub implementation that immediately returns success. This FMU template contains no integer variables, so this function performs no operations. Included for FMI 2.0 standard compliance.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance (unused) |
| `vr` | `const fmi2ValueReference[]` | Array of value references (unused) |
| `nvr` | `size_t` | Number of value references (unused) |
| `value` | `const fmi2Integer[]` | Array of values (unused) |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Always returns success |

#### Notes
- **Stub implementation**: No integer variables exist in this template
- **Extension point**: Add integer handling here if your model needs it

---

### fmi2GetInteger

Reads integer-valued variables (not used in this template).

#### Signature
```c
fmi2Status fmi2GetInteger(fmi2Component c,
                          const fmi2ValueReference vr[],
                          size_t nvr,
                          fmi2Integer value[]);
```

#### Description
Stub implementation that immediately returns success. This FMU template contains no integer variables.

#### Parameters
| Name | Type | Description |
|------|------|-------------|
| `c` | `fmi2Component` | The FMU instance (unused) |
| `vr` | `const fmi2ValueReference[]` | Array of value references (unused) |
| `nvr` | `size_t` | Number of value references (unused) |
| `value` | `fmi2Integer[]` | Output array (unused) |

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Always returns success |

#### Notes
- **Stub implementation**: No integer variables exist in this template

---

### fmi2SetBoolean

Sets boolean-valued variables (not used in this template).

#### Signature
```c
fmi2Status fmi2SetBoolean(fmi2Component c,
                          const fmi2ValueReference vr[],
                          size_t nvr,
                          const fmi2Boolean value[]);
```

#### Description
Stub implementation. No boolean variables in this template.

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Always returns success |

---

### fmi2GetBoolean

Reads boolean-valued variables (not used in this template).

#### Signature
```c
fmi2Status fmi2GetBoolean(fmi2Component c,
                          const fmi2ValueReference vr[],
                          size_t nvr,
                          fmi2Boolean value[]);
```

#### Description
Stub implementation. No boolean variables in this template.

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Always returns success |

---

### fmi2SetString

Sets string-valued variables (not used in this template).

#### Signature
```c
fmi2Status fmi2SetString(fmi2Component c,
                         const fmi2ValueReference vr[],
                         size_t nvr,
                         const fmi2String value[]);
```

#### Description
Stub implementation. No string variables in this template.

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Always returns success |

---

### fmi2GetString

Reads string-valued variables (not used in this template).

#### Signature
```c
fmi2Status fmi2GetString(fmi2Component c,
                         const fmi2ValueReference vr[],
                         size_t nvr,
                         fmi2String value[]);
```

#### Description
Stub implementation. No string variables in this template.

#### Return Values
| Status | Condition |
|--------|-----------|
| `fmi2OK` | Always returns success |

---