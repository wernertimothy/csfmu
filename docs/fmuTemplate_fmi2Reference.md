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