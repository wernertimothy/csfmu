
#==============================================================================
# 1. Project Setup
#==============================================================================

cmake_minimum_required(VERSION 3.12)
project(csfmu VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

#==============================================================================
# 2. Build Configuration
#==============================================================================

# Platform detection for FMU binaries
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(FMI_ARCH "aarch64")
else()
    set(FMI_ARCH "x86_64")
endif()

if(WIN32)
    set(FMI_PLATFORM "${FMI_ARCH}-windows")
elseif(APPLE)
    set(FMI_PLATFORM "${FMI_ARCH}-darwin")
else()
    set(FMI_PLATFORM "${FMI_ARCH}-linux")
endif()
message(STATUS "Build platform: ${FMI_PLATFORM}")

# Model selection (User can override with -DMODEL=<model_name>)
if(NOT DEFINED MODEL)
    set(MODEL "first_order")
    message(STATUS "No model specified, defaulting to: ${MODEL}")
else()
    message(STATUS "Building model: ${MODEL}")
endif()

# Library name matches the model name (produces first_order.dylib/.so/.dll)
set(LIB_NAME "${MODEL}")

# Output directory for packaged FMU (User can override with -DFMU_OUTPUT_DIR=<path>)
set(FMU_OUTPUT_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "Directory where the packaged .fmu file is placed")

#==============================================================================
# 3. Source Files
#==============================================================================

# Core FMU
set(FMU_SOURCES
    src/fmi2Functions.c
)

# Model core (FMU-agnostic and model-agnostic)
set(CORE_SOURCES
    src/model.c
    src/integrator.c
)

# Selected model (FMU-agnostic)
# Auto-detect C or C++ model equations
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/models/${MODEL}/equations.cpp")
    set(MODEL_EQUATIONS "src/models/${MODEL}/equations.cpp")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/models/${MODEL}/equations.c")
    set(MODEL_EQUATIONS "src/models/${MODEL}/equations.c")
else()
    message(FATAL_ERROR "No equations.c or equations.cpp found for model '${MODEL}'")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/models/${MODEL}/variables.c")
    message(FATAL_ERROR "No variables.c found for model '${MODEL}'")
endif()

# The model is selected at link time: variables.c defines MODEL_DEFINITION
set(MODEL_SOURCES
    ${CORE_SOURCES}
    src/models/${MODEL}/variables.c
    ${MODEL_EQUATIONS}
)

#==============================================================================
# 4. FMU Shared Library
#==============================================================================

# Build the FMU as a shared library (DLL/SO)
add_library(${LIB_NAME} SHARED
    ${FMU_SOURCES}
    ${MODEL_SOURCES}
)

target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fmi2/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
)

# Set the output name to match FMI requirements
# Only the fmi2* functions are exported (marked by FMI2_Export in the FMI
# header). Everything else is hidden, otherwise two FMUs of this template
# loaded into one process could bind to each other's MODEL_DEFINITION.
set_target_properties(${LIB_NAME} PROPERTIES
    PREFIX ""
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Bind the library's own calls to fmi2* functions to itself (ELF only), so
# they cannot be redirected to another FMU loaded with RTLD_GLOBAL
if(UNIX AND NOT APPLE)
    set_property(TARGET ${LIB_NAME} APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bsymbolic-functions")
endif()

# Math library (not part of the C library on Linux)
if(UNIX)
    target_link_libraries(${LIB_NAME} PRIVATE m)
endif()

# Windows-specific settings
if(WIN32)
    target_compile_definitions(${LIB_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

#==============================================================================
# 5. FMU Static Library for examples/test
#==============================================================================

add_library(${LIB_NAME}_static STATIC
    ${FMU_SOURCES}
    ${MODEL_SOURCES}
)

target_include_directories(${LIB_NAME}_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fmi2/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
)

if(UNIX)
    target_link_libraries(${LIB_NAME}_static PUBLIC m)
endif()

#==============================================================================
# 6. Model core static library for unit tests (no model, no FMI)
#==============================================================================

add_library(model_core STATIC
    ${CORE_SOURCES}
)

target_include_directories(model_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#==============================================================================
# 7. Subdirectories
#==============================================================================

add_subdirectory(examples)
add_subdirectory(test/unit)

#==============================================================================
# 8. modelDescription.xml generation
#==============================================================================

# The generator is linked against the selected model and writes the
# modelDescription.xml from its MODEL_DEFINITION. It runs on the build
# machine, so cross-compiling is not supported.
add_executable(generate_model_description
    src/fmi2ModelDescription.c
)

target_link_libraries(generate_model_description PRIVATE ${LIB_NAME}_static)

set(MODEL_DESCRIPTION_XML "${CMAKE_BINARY_DIR}/modelDescription.xml")

add_custom_command(
    OUTPUT ${MODEL_DESCRIPTION_XML}
    COMMAND generate_model_description ${MODEL_DESCRIPTION_XML}
    DEPENDS generate_model_description
    COMMENT "Generating modelDescription.xml for model '${MODEL}' ..."
    VERBATIM
)

add_custom_target(model_description DEPENDS ${MODEL_DESCRIPTION_XML})

# The FMU can only be packaged once the modelDescription.xml exists
add_dependencies(${LIB_NAME} model_description)

#==============================================================================
# 9. FMU packaging
#==============================================================================

# Install shared library to FMU binaries direction
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION binaries/${FMI_PLATFORM}
    RUNTIME DESTINATION binaries/${FMI_PLATFORM}
)

# Find python
if(WIN32)
    set(VENV_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/.venv/Scripts/python.exe")
else()
    set(VENV_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python3")
endif()

if(EXISTS ${VENV_PYTHON})
    set(Python3_EXECUTABLE ${VENV_PYTHON})
    message(STATUS "Using virtual environment Python: ${VENV_PYTHON}")
else()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    message(STATUS "Using system Python: ${Python3_EXECUTABLE}")
endif()

# Package FMU
add_custom_command(
    TARGET ${LIB_NAME} POST_BUILD
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_fmu.py
        $<TARGET_FILE:${LIB_NAME}>
        ${MODEL_DESCRIPTION_XML}
        ${FMU_OUTPUT_DIR}
        ${LIB_NAME}
    COMMENT "Packaging FMU to ${FMU_OUTPUT_DIR}/${LIB_NAME}.fmu ..."
    VERBATIM
)