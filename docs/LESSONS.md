# Lessons Learned

A summary of my lessons learned during this project.

## C concepts

### Context Pointer pattern

Callbacks can be called from a variety of different contexts.
The solution to handle this problem is to provide a context to the logger function.

#### Example Logger

The integration tool has to handle variable number of different FMUs.
To be able to distinguish them and get information about the specific FMUs that call the logger the **componentEnvironment** pointer can be used to handle FMU specific context to the logger function.

```C
 typedef struct {
      FILE* logFile;
      int logLevel;
      const char* fmuName;
  } MySimulatorContext;

  MySimulatorContext ctx = {
      .logFile = fopen("fmu1_logs.txt", "w"),
      .logLevel = 2,
      .fmuName = "CarEngine"
  };

  fmi2CallbackFunctions callbacks = {
      .componentEnvironment = &ctx,  // ← Pass your context
      .logger = myLogger,
      // ...
  };

  Then in your logger callback:

  void myLogger(fmi2ComponentEnvironment componentEnvironment,
                fmi2String instanceName,
                fmi2Status status,
                fmi2String category,
                fmi2String message, ...) {

      // Cast back to your context type
      MySimulatorContext* ctx = (MySimulatorContext*)componentEnvironment;

      // Now you have access to your simulator's state!
      fprintf(ctx->logFile, "[%s] %s: %s\n",
              ctx->fmuName, category, message);
  }
```

### Variadic Arguments

This allows to pass a variable number of arguments to a function.
This is helpfull if the number of arguments varies, for example the arguments of a format string when calling a log function.

#### Example format string

```C
#include <stdarg.h>

// ...

void logMessage(const char* message, ...) {
    va_list args;               // init args
    va_start(args, message);    // get variable args after message 
    vprintf(message, args);     
    printf("\n");
    va_end(args);               // close args
}

// ...
```

### Format String security

printf(message) treats message as a format string.
If message contains "%i", the compiler will start looking for an integer on the stack.
Since non is provided, this can cause unpredictable behaviour and lead to security issues.
This is a security vulnerability - attackers can craft messages to read/write memory
Compilers warn about this because it's a common exploit vector.

Option 1: Treat message as plain text (simple, safe for now)
```C
printf("%s", message);
```
This says: "Print the string message verbatim - don't interpret format specifiers"

Option 2: Support format strings properly (for later when you add variadic args)
```C
va_list args;
va_start(args, message);
vprintf(message, args);  // Now message CAN have %d, %s, etc.
va_end(args);
```

### Circular includes

You get a circular dependency if two files want to include each other.
As a rule of thumb, the include hirarchy should be a directed acyclic graph, not a circle.

If you have co-dependency, forward declare types

```C
typedef struct modelInstance modelInstance;
```

or assume the types are declared.

### Relative includes

Try to prevent relative includes.
Instead ad the correct path to the build system and include from there

```c
#include "../src/models/firsOrder/model.h" // don't
#include "firstOrder/model.h"              // configure CMake to add -I src/models
```

### Code duplication

Figure out what the core functionality is, factor that out and then implement wrapper functions that parametrize the core.

```C
/**
 * core
 */
static inline void logMessage(
    myFMU2* fmu,
    logCategory category, 
    fmi2Status status, 
    const char* categoryName,
    fmi2String message, 
    va_list args) {
    if (logCategoryIsActive(fmu->logLevel, category)) {
        char buffer[LOG_BUFFER_SIZE];
        vsnprintf(buffer, sizeof(buffer), message, args);
        fmu->logger(
            fmu->componentEnvironment,
            fmu->instanceName,
            status,
            categoryName,
            buffer
        );
    }
}

/**
 * wrapper
 */
static inline void logErrorMessage(myFMU2* fmu, fmi2String message, ...) {
    va_list args;
    va_start(args, message);
    logMessage(fmu, logStatusError, fmi2Error, LOG_ERROR, message, args);
    va_end(args);
}
```


### Memory Management Patterns

**Use callbacks for allocation in FMI context:**
```C
// In fmi2Instantiate - use integration tool's allocator
myFMU2* myfmu2 = functions->allocateMemory(1, sizeof(myFMU2));
// Match with corresponding free
functions->freeMemory(myfmu2);
```

**Zero-initialize after allocation:**
```C
memset(myfmu2, 0, sizeof(myFMU2));  // Prevents uninitialized pointer bugs
```

**Free in reverse allocation order:**
```C
// Allocate: wrapper → model → instanceName
// Free: instanceName → model → wrapper
freeMemory((void*)myfmu2->instanceName);  // Cast away const
freeModel(myfmu2->model);
freeMemory(myfmu2);
```

**NULL-safe cleanup:**
```C
if (c) {  // Check before dereferencing
    myFMU2* myfmu2 = (myFMU2*)c;
    // ... cleanup
}
```

### String Handling

**strcmp returns 0 on match (counterintuitive):**
```C
if (strcmp(guid, FMU_GUID) != 0) {  // Different!
    // Error: GUID mismatch
}
```

**Duplicate strings for ownership:**
```C
myfmu2->instanceName = strdup(instanceName);  // Own the copy
if (!myfmu2->instanceName) {
    // Handle allocation failure
}
```

### Bitwise Operations

**Bitmask for flags:**
```C
typedef enum {
    logNone = 0,
    logEvents = 1 << 0,    // 0001
    logError = 1 << 1,     // 0010
    logAll = (1 << 9) - 1  // 111111111 (9 bits set)
} logCategory;
```

**Bitwise OR for accumulation:**
```C
logLevel |= logEvents;  // Add flag (idempotent)
```

**Bitwise AND for checking:**
```C
if (logLevel & logEvents) {  // Is this flag set?
    // Log the event
}
```

**Power-of-2 enum values for state tracking:**
```C
typedef enum {
    stateInstantiated = 1 << 0,
    stateInitialized = 1 << 1,
    // NOT stateInstantiated = 0  // Breaks bitwise AND checks!
} fmuState;
```

### State Management

**Mutually exclusive states - use assignment:**
```C
state = stateInitializationMode;  // Replaces old state completely
```

**Error states - use bitwise OR:**
```C
state |= stateError;  // Overlay error, preserve current state
```

**State validation with bitwise AND:**
```C
if (!(myfmu2->state & stateInstantiated)) {
    // Not in allowed state
    return fmi2Error;
}
```

### Array Operations

**memcpy for bulk initialization:**
```C
memcpy(model->states, defaultStates, nStates * sizeof(double));
```

**Index arithmetic for value references:**
```C
// VR layout: [params 0-1][inputs 2-3][outputs 4-5]
if (vr < NUMBER_OF_PARAMS) {
    model->parameter[vr] = value;
} else if (vr < NUMBER_OF_PARAMS + NUMBER_OF_INPUTS) {
    model->inputs[vr - NUMBER_OF_PARAMS] = value;  // Offset by params
}
```

**Integer loop counters for robustness:**
```C
// GOOD: No floating-point accumulation errors
int nSteps = (int)(stepSize / MAX_STEP);
for (int k = 0; k < nSteps; k++) { /* ... */ }

// BAD: Accumulates floating-point errors
double remaining = stepSize;
while (remaining > 0) { remaining -= MAX_STEP; }
```

### Function Pointers

**Macro to function pointer:**
```C
// config.h: #define MODEL_STATE_EQUATION firstOrderSystem
model->stateEquation = MODEL_STATE_EQUATION;  // Expands to function name
```

### Defensive Programming

**Unused parameter suppression:**
```C
(void)tolerance;  // Portable way to mark intentionally unused
```

**Defensive initialization:**
```C
double actualStepSize = 0.0;  // Initialize before use
```

**Epsilon-based thresholds:**
```C
#define EPSILON 1e-9
if (remainder > EPSILON) {  // Skip insignificant remainders
    integrate(remainder);
}
```

## FMU Concepts

### FMI Lifecycle

**Inquiry functions don't require instance:**
```C
// Can call before fmi2Instantiate
const char* version = fmi2GetVersion();
const char* platform = fmi2GetTypesPlatform();
```

**GUID validation prevents wrong binary:**
```C
if (strcmp(guid, FMU_GUID) != 0) {
    logger(..., "GUID mismatch - wrong FMU binary loaded");
    return NULL;  // Reject instantiation
}
```

**Two-phase initialization (defaults then overrides):**
```C
// 1. FMU sets defaults in EnterInitializationMode
model->parameter[0] = 1.0;  // Default tau
// 2. Integration tool can override before ExitInitializationMode
fmi2SetReal(c, vr, 1, &customTau);
// 3. Finalize in ExitInitializationMode
```

**Two-phase shutdown:**
```C
fmi2Terminate(c);      // Logical shutdown (close files, etc.)
fmi2FreeInstance(c);   // Physical cleanup (free memory)
```

### State Machine

**Normal states are mutually exclusive:**
```C
// FMU is in ONE of: Instantiated, Initialization, StepComplete, etc.
state = stateInitializationMode;  // Simple assignment
```

**Error states overlay current state:**
```C
state |= stateError;  // ADD error flag, keeps current state info
// Now state is "InitializationMode AND Error"
```

**State matrix uses OR logic:**
```C
// FMI spec: function allowed if in ANY listed state
if (state & (stateInitialized | stateStepComplete)) {
    // Can call fmi2DoStep in either state
}
```

### Variable Access

**Sequential VR mapping for efficiency:**
```C
// modelDescription.xml order: [params][inputs][outputs]
// VR 0-1: parameters
// VR 2-3: inputs
// VR 4-5: outputs
// Direct array indexing, no hash table needed
```

**State-dependent settability:**
```C
// Parameters: only in initialization mode
// Inputs: anytime after initialization starts
// Outputs: read-only (set by FMU internally)
```

**Bulk operations reduce overhead:**
```C
// Instead of 100 function calls:
for (int i = 0; i < 100; i++) fmi2SetReal(c, vr[i], 1, &val[i]);
// One function call:
fmi2SetReal(c, vr, 100, val);
```

### Co-Simulation Specifics

**Tolerance ignored in co-simulation:**
```C
(void)tolerance;  // Master controls accuracy via step size
// Model Exchange FMUs would use this for internal solver
```

**Communication step ≠ integration step:**
```C
// Integration tool requests: 0.01s step
// FMU subdivides: 10x 0.001s internal steps for stability
int nSteps = (int)(commStep / MAX_SOLVER_STEP);
```

**Zero-order hold on inputs:**
```C
// FMI assumes inputs constant during communication step
// No interpolation - discrete co-simulation assumption
```

**Sub-stepping for stability:**
```C
// Explicit integrators have stability limits
double maxStep = 1e-3;  // Euler Forward stability bound
if (communicationStep > maxStep) {
    // Subdivide into stable steps
}
```

### Callbacks

**componentEnvironment can be NULL:**
```C
// Optional context for callbacks - don't reject NULL
if (componentEnvironment) {
    // Use it if provided
}
```

**Logger must be valid before any operation:**
```C
// Check logger first - needed for error reporting
if (!functions->logger) {
    return NULL;  // Can't report errors without logger
}
```

**Store callbacks immediately for error recovery:**
```C
myfmu2->logger = functions->logger;
myfmu2->allocateMemory = functions->allocateMemory;
// Now error paths can use logger and freeMemory
```

### Model Agnostic Design

**Keep model layer FMU-agnostic:**
```C
// model.c uses standard malloc/free
modelInstance* newModel(int nStates, int nInputs) {
    return malloc(sizeof(modelInstance));  // NOT callbacks
}
// FMI layer uses callbacks
myFMU2* myfmu2 = functions->allocateMemory(1, sizeof(myFMU2));
```

**Function pointers enable polymorphism:**
```C
// Different models via compile-time configuration
model->stateEquation = MODEL_STATE_EQUATION;  // From config.h
model->integrate = INTEGRATOR_METHOD;         // Swappable integrator
```

## Build System Concepts

### Organization
1. Project Setup
2. Build Confoiguration
3. Source Files
4. Libraries/Targets
5. Subdirectories
6. Packaging

### Dependency Propagation
* **PRIVATE** : I need thid to compile but users don't
* **PUBLIC** : I need this to compile and user do
* **INTERFACE** : I don't need this to compile but my users do

#### Example from Project

```CMake
target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fmi2/src
)
```

Why PUBLIC here?
1. The FMU library needs fmi2Functions.h to compile fmi2Functions.c
2. Any code using the FMU (like example_fmu.c) also needs fmi2Functions.h to call FMI functions

