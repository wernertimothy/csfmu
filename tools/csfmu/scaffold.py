"""Scaffolding of new models: name validation and the template files."""

import keyword
import re
import uuid
from pathlib import Path
from string import Template

MAX_NAME_LENGTH = 64

NAME_PATTERN = re.compile(r"[A-Za-z][A-Za-z0-9_]*")

# The model name becomes the FMI modelIdentifier, which must be a valid C
# function name, and the equations may be written in C++
C_KEYWORDS = {
    "auto",
    "break",
    "case",
    "char",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "float",
    "for",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "register",
    "restrict",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while",
}
CPP_KEYWORDS = {
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "bitand",
    "bitor",
    "bool",
    "catch",
    "char8_t",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "concept",
    "const_cast",
    "consteval",
    "constexpr",
    "constinit",
    "co_await",
    "co_return",
    "co_yield",
    "decltype",
    "delete",
    "dynamic_cast",
    "explicit",
    "export",
    "false",
    "friend",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "reinterpret_cast",
    "requires",
    "static_assert",
    "static_cast",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typeid",
    "typename",
    "using",
    "virtual",
    "wchar_t",
    "xor",
    "xor_eq",
}

# The model name is also the name of a CMake target, so it must not collide
# with the built-in targets (incl. those of Visual Studio) or the other targets
# of this project. Compared in lowercase: file names of targets that only differ
# in case collide on macOS and Windows.
RESERVED_TARGETS = {
    "all",
    "clean",
    "help",
    "install",
    "test",
    "package",
    "edit_cache",
    "rebuild_cache",
    "all_build",
    "zero_check",
    "run_tests",
    "model_core",
    "unity",
    "model_description",
    "generate_model_description",
    "example_model",
    "example_fmu",
}

# Folder names that Windows does not allow, in any case
WINDOWS_RESERVED = {
    "con",
    "prn",
    "aux",
    "nul",
    *(f"com{i}" for i in range(1, 10)),
    *(f"lpt{i}" for i in range(1, 10)),
}


def validate_name(name: str) -> str | None:
    """Return why the name cannot be used for a model, or None if it can.

    The name is used as folder name, CMake target, FMI modelIdentifier,
    file name of the FMU and Python package name of the model's tests.
    """
    hint = "use letters, digits and underscores, starting with a letter (e.g. my_model)"
    if not NAME_PATTERN.fullmatch(name):
        return f"invalid model name '{name}': {hint}"
    if len(name) > MAX_NAME_LENGTH:
        return f"invalid model name '{name}': longer than {MAX_NAME_LENGTH} characters"
    if "__" in name or name.endswith("_"):
        return f"invalid model name '{name}': no double or trailing underscores (e.g. my_model)"
    if name in C_KEYWORDS or name in CPP_KEYWORDS or keyword.iskeyword(name):
        return f"invalid model name '{name}': it is a C, C++ or Python keyword"
    if name.lower() in RESERVED_TARGETS or name.lower() in WINDOWS_RESERVED:
        return f"invalid model name '{name}': the name is reserved by the build system or the OS"
    return None


def camel_case(name: str) -> str:
    """Return the snake_case name in camelCase: first_order -> firstOrder, my_PID -> myPID."""
    first, *rest = name.split("_")
    return first + "".join(part[:1].upper() + part[1:] for part in rest)


VARIABLES_H = Template("""\
#ifndef ${guard}
#define ${guard}

#include "model.h"

/* parameters */
typedef struct {
    double k;    /* integrator gain */
} Params;

/* inputs */
typedef struct {
    double u;    /* input signal */
} Inputs;

/* outputs */
typedef struct {
    double y;    /* integrated signal */
} Outputs;

/* continuous states (doubles only) */
typedef struct {
    double x;    /* state of the integrator */
} States;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void ${camel}System(modelInstance* model);

/* output equation */
void ${camel}Outputs(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
""")

VARIABLES_C = Template("""\
#include "variables.h"
#include "integrator.h"

/* published variables (order is free) */
static const modelVariable VARIABLES[] = {
    { PARAM(k),  .start = 1.0, .unit = "1", .min = SOME(0.0), .max = SOME(100.0), .nominal = 1.0,
      .description = "Gain of the integrator" },
    { INPUT(u),  .start = 0.0, .unit = "1",
      .description = "Input signal to the integrator" },
    { OUTPUT(y),               .unit = "1", .dependsOn = "",
      .description = "Output of the integrator (equal to state x)" },
    { STATE(x),  .start = 0.0, .unit = "1",
      .description = "State of the integrator" },
};

/* units in terms of SI base units */
static const modelUnit UNITS[] = {
    { .name = "1" },
};

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "${name}",
    .guid        = "{${guid}}",
    .description = "Integrator with gain: dx/dt = k*u",
    .version     = "1.0.0",

    /* variables and units */
    .variables         = VARIABLES,
    .numberOfVariables = ARRAY_LEN(VARIABLES),
    .units             = UNITS,
    .numberOfUnits     = ARRAY_LEN(UNITS),

    /* system dimensions */
    .sizeOfParams   = sizeof(Params),
    .sizeOfInputs   = sizeof(Inputs),
    .sizeOfOutputs  = sizeof(Outputs),
    .numberOfStates = NUMBER_OF_DOUBLES(States),

    /* model equations */
    .getStateDerivatives = ${camel}System,
    .getOutputs          = ${camel}Outputs,

    /* method for numerical integration */
    .integrator  = &FORWARD_EULER,
    .maxStepSize = 1e-3,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.01 },
};
""")

EQUATIONS_C = Template("""\
#include "variables.h"

/**
 * Integrator state equation: dx/dt = f(t, x, u)
 *
 * Implements the differential equation:
 *     dx/dt = k*u
 *
 * where:
 *     k = integrator gain (parameter)
 *     u = input signal (input)
 *     x = state variable (state)
 *
 * Implementation notes:
 * - Views the storage blocks through the structs from variables.h
 * - The current time t is model->time (this equation does not use it)
 * - The state vector is a double array; States gives its entries names
 * - Writes result directly to the state derivatives
 */
void ${camel}System(modelInstance* model)
{
    const Params* p  = model->params;
    const Inputs* in = model->inputs;
    States*       dx = (States*)model->stateDerivatives;

    /* Compute state derivative: dx/dt = k*u */
    dx->x = p->k * in->u;
}

/**
 * Integrator output equation: y = h(t, x, u)
 *
 * The output is equal to the state:
 *     y = x
 *
 * Implementation notes:
 * - No direct feedthrough of u, hence .dependsOn = "" in variables.c
 */
void ${camel}Outputs(modelInstance* model)
{
    const States* x   = (const States*)model->states;
    Outputs*      out = model->outputs;

    /* Output equals state (y = x) */
    out->y = x->x;
}
""")

EQUATIONS_CPP = Template("""\
#include "variables.h"

/* The equations are called from C, so they need C linkage */
extern "C" {

/**
 * Integrator state equation: dx/dt = f(t, x, u)
 *
 * Implements the differential equation:
 *     dx/dt = k*u
 *
 * where:
 *     k = integrator gain (parameter)
 *     u = input signal (input)
 *     x = state variable (state)
 *
 * Implementation notes:
 * - Views the storage blocks through the structs from variables.h. C++ does
 *   not convert void* implicitly, so the blocks need explicit casts
 *   (reinterpret_cast for the state vectors, which are double arrays)
 * - The current time t is model->time (this equation does not use it)
 * - The state vector is a double array; States gives its entries names
 * - Writes result directly to the state derivatives
 */
void ${camel}System(modelInstance* model)
{
    const Params& p  = *static_cast<const Params*>(model->params);
    const Inputs& in = *static_cast<const Inputs*>(model->inputs);
    States&       dx = *reinterpret_cast<States*>(model->stateDerivatives);

    /* Compute state derivative: dx/dt = k*u */
    dx.x = p.k * in.u;
}

/**
 * Integrator output equation: y = h(t, x, u)
 *
 * The output is equal to the state:
 *     y = x
 *
 * Implementation notes:
 * - No direct feedthrough of u, hence .dependsOn = "" in variables.c
 */
void ${camel}Outputs(modelInstance* model)
{
    const States& x   = *reinterpret_cast<const States*>(model->states);
    Outputs&      out = *static_cast<Outputs*>(model->outputs);

    /* Output equals state (y = x) */
    out.y = x.x;
}

}  /* extern "C" */
""")

TEST_MODEL = Template('''\
"""Tests of the ${name} model.

The template model is an integrator with gain: dx/dt = k*u, y = x. Adapt the
tests when you change the equations, and check each result against a known
solution, as the tests of the demo models in tests/models/ do.

The tests in tests/integration/ check the FMI behavior of every model. The
tests in this folder are only collected when the ${name} model is tested.
"""

import pytest

from tests.fmu_helpers import advance


def test_output_integrates_the_input(initialized_fmu, vr) -> None:
    """For a constant input the output grows linearly: y(t) = k*u*t."""
    fmu = initialized_fmu
    k = fmu.getReal([vr("k")])[0]
    fmu.setReal([vr("u")], [0.5])

    end = advance(fmu, 1.0)

    assert fmu.getReal([vr("y")])[0] == pytest.approx(k * 0.5 * end, abs=1e-12)
''')


# The bare templates: every storage block, every field of the model definition
# and every function, all of them empty. C does not allow empty structs, and an
# FMU needs at least one variable, so a bare model is not a valid model until
# the user has filled in what the model needs and deleted the rest.

VARIABLES_H_BARE = Template("""\
#ifndef ${guard}
#define ${guard}

#include "model.h"

/*
 * Fill in the structs your model needs and delete the others, together with
 * their line in MODEL_DEFINITION (variables.c). C does not allow empty
 * structs: GCC and Clang accept them as an extension, MSVC refuses them.
 */

/* parameters and calculated parameters (double, int, bool, char name[MODEL_STRING_SIZE]) */
typedef struct {
} Params;

/* inputs (double, int, bool) */
typedef struct {
} Inputs;

/* outputs (double, int, bool) */
typedef struct {
} Outputs;

/* local variables: published read-only, computed by the output equation (double, int, bool) */
typedef struct {
} Locals;

/* continuous states (doubles only) */
typedef struct {
} States;

/* internals: not published, part of the FMU state (plain values and fixed-size arrays) */
typedef struct {
} Internals;

#ifdef __cplusplus
extern "C" {
#endif

/* state equation */
void ${camel}System(modelInstance* model);

/* output equation, also computes the local variables */
void ${camel}Outputs(modelInstance* model);

/* calculated parameters (optional) */
void ${camel}CalculateParameters(modelInstance* model);

/* initialization (optional) */
simStatus ${camel}Initialize(modelInstance* model);

#ifdef __cplusplus
}
#endif

#endif
""")

VARIABLES_C_BARE = Template("""\
#include "variables.h"
#include "integrator.h"

const modelDefinition MODEL_DEFINITION = {
    /* model identifier */
    .name        = "${name}",
    .guid        = "{${guid}}",
    .description = "",
    .version     = "1.0.0",

    /* variables and units (no tables yet; an FMU needs at least one variable) */
    .variables         = NULL,
    .numberOfVariables = 0,
    .units             = NULL,
    .numberOfUnits     = 0,

    /* system dimensions */
    .sizeOfParams    = sizeof(Params),
    .sizeOfInputs    = sizeof(Inputs),
    .sizeOfOutputs   = sizeof(Outputs),
    .sizeOfLocals    = sizeof(Locals),
    .sizeOfInternals = sizeof(Internals),
    .numberOfStates  = NUMBER_OF_DOUBLES(States),

    /* model equations */
    .getStateDerivatives = ${camel}System,
    .getOutputs          = ${camel}Outputs,

    /* method for numerical integration */
    .integrator  = &FORWARD_EULER,
    .maxStepSize = 1e-3,

    /* calculated parameters and initialization (optional) */
    .calculateParameters = ${camel}CalculateParameters,
    .initialize          = ${camel}Initialize,

    /* suggested simulation settings */
    .defaultExperiment = { .startTime = 0.0, .stopTime = 10.0, .tolerance = 1e-6, .stepSize = 0.01 },
};
""")

EQUATIONS_C_BARE = Template("""\
#include "variables.h"

/**
 * State equation: dx/dt = f(t, x, u)
 */
void ${camel}System(modelInstance* model)
{
    (void)model;
    /* const Params*    p         = model->params; */
    /* const Inputs*    in        = model->inputs; */
    /* const States*    x         = (const States*)model->states; */
    /* const Internals* internals = model->internals; */
    /* States*          dx        = (States*)model->stateDerivatives; */
}

/**
 * Output equation: y = h(t, x, u), also computes the local variables
 */
void ${camel}Outputs(modelInstance* model)
{
    (void)model;
    /* const Params*    p         = model->params; */
    /* const Inputs*    in        = model->inputs; */
    /* const States*    x         = (const States*)model->states; */
    /* const Internals* internals = model->internals; */
    /* Outputs*         out       = model->outputs; */
    /* Locals*          locals    = model->locals; */
}

/**
 * Calculated parameters: computed from the parameters only
 */
void ${camel}CalculateParameters(modelInstance* model)
{
    (void)model;
    /* Params* p = model->params; */
}

/**
 * Initialization: may set the states with .initial = VAR_APPROX and the internals
 */
simStatus ${camel}Initialize(modelInstance* model)
{
    (void)model;
    /* const Params* p         = model->params; */
    /* const Inputs* in        = model->inputs; */
    /* States*       x         = (States*)model->states; */
    /* Internals*    internals = model->internals; */
    return SIM_OK;
}
""")

EQUATIONS_CPP_BARE = Template("""\
#include "variables.h"

/* The equations are called from C, so they need C linkage */
extern "C" {

/**
 * State equation: dx/dt = f(t, x, u)
 */
void ${camel}System(modelInstance* model)
{
    (void)model;
    // const Params&    p         = *static_cast<const Params*>(model->params);
    // const Inputs&    in        = *static_cast<const Inputs*>(model->inputs);
    // const States&    x         = *reinterpret_cast<const States*>(model->states);
    // const Internals& internals = *static_cast<const Internals*>(model->internals);
    // States&          dx        = *reinterpret_cast<States*>(model->stateDerivatives);
}

/**
 * Output equation: y = h(t, x, u), also computes the local variables
 */
void ${camel}Outputs(modelInstance* model)
{
    (void)model;
    // const Params&    p         = *static_cast<const Params*>(model->params);
    // const Inputs&    in        = *static_cast<const Inputs*>(model->inputs);
    // const States&    x         = *reinterpret_cast<const States*>(model->states);
    // const Internals& internals = *static_cast<const Internals*>(model->internals);
    // Outputs&         out       = *static_cast<Outputs*>(model->outputs);
    // Locals&          locals    = *static_cast<Locals*>(model->locals);
}

/**
 * Calculated parameters: computed from the parameters only
 */
void ${camel}CalculateParameters(modelInstance* model)
{
    (void)model;
    // Params& p = *static_cast<Params*>(model->params);
}

/**
 * Initialization: may set the states with .initial = VAR_APPROX and the internals
 */
simStatus ${camel}Initialize(modelInstance* model)
{
    (void)model;
    // const Params& p         = *static_cast<const Params*>(model->params);
    // const Inputs& in        = *static_cast<const Inputs*>(model->inputs);
    // States&       x         = *reinterpret_cast<States*>(model->states);
    // Internals&    internals = *static_cast<Internals*>(model->internals);
    return SIM_OK;
}

}  /* extern "C" */
""")

TEST_MODEL_BARE = Template('''\
"""Tests of the ${name} model.

Add the tests of your equations here, and check each result against a known
solution, as the tests of the demo models in tests/models/ do.

The tests in tests/integration/ check the FMI behavior of every model. The
tests in this folder are only collected when the ${name} model is tested.
"""
''')


def create_model(
    root: Path, name: str, *, cpp: bool = False, bare: bool = False
) -> list[Path]:
    """Create the files of a new model from the template and return their paths.

    Creates src/models/<name>/ with an integrator (dx/dt = k*u, y = x) and
    tests/models/<name>/ with a starter test of the model. Neither folder may exist.
    With cpp, the equations are written in C++ (equations.cpp instead of
    equations.c); variables.c stays C, because the variable table needs C.
    With bare, the files are an empty skeleton instead of the integrator:
    empty structs, every field of the model definition, empty functions and
    a test file without tests. It is not a valid model until it is filled in.
    """
    if bare:
        variables_h, variables_c, test = (
            VARIABLES_H_BARE,
            VARIABLES_C_BARE,
            TEST_MODEL_BARE,
        )
        equations = EQUATIONS_CPP_BARE if cpp else EQUATIONS_C_BARE
    else:
        variables_h, variables_c, test = VARIABLES_H, VARIABLES_C, TEST_MODEL
        equations = EQUATIONS_CPP if cpp else EQUATIONS_C
    equations_file = "equations.cpp" if cpp else "equations.c"
    values = {
        "name": name,
        "camel": camel_case(name),
        "guard": f"{name.upper()}_VARIABLES_H",
        "guid": str(uuid.uuid4()),
    }
    model_dir = root / "src" / "models" / name
    test_dir = root / "tests" / "models" / name
    files = {
        model_dir / "variables.h": variables_h.substitute(values),
        model_dir / "variables.c": variables_c.substitute(values),
        model_dir / equations_file: equations.substitute(values),
        test_dir / "__init__.py": "",
        test_dir / f"test_{name}.py": test.substitute(values),
    }

    model_dir.mkdir(parents=True)
    test_dir.mkdir(parents=True)
    for path, content in files.items():
        # newline="\n" keeps LF line endings on Windows too
        with path.open("x", encoding="utf-8", newline="\n") as file:
            file.write(content)
    return list(files)
