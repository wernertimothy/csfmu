
#==============================================================================
# 1. Project Setup
#==============================================================================

cmake_minimum_required(VERSION 3.12)
project(fmuTemplate VERSION 1.0.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

# Normalize model name to uppercase for preprocessor define
string(TOUPPER ${MODEL} MODEL_UPPER)
set(MODEL_DEFINE "MODEL_${MODEL_UPPER}")

# Output directory for packaged FMU (User can override with -DFMU_OUTPUT_DIR=<path>)
set(FMU_OUTPUT_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "Directory where the packaged .fmu file is placed")

#==============================================================================
# 3. Source Files
#==============================================================================

# Core FMU
set(FMU_SOURCES
    src/fmi2Functions.c
)

# Model (FMU-agnostic)
set(MODEL_SOURCES
    src/model.c
    src/integrator.c
    src/models/${MODEL}/equations.c
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

target_compile_definitions(${LIB_NAME} PRIVATE
    ${MODEL_DEFINE}
)

# Set the output name to match FMI requirements
set_target_properties(${LIB_NAME} PROPERTIES
    PREFIX ""
    POSITION_INDEPENDENT_CODE ON
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(${LIB_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(${LIB_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
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

target_compile_definitions(${LIB_NAME}_static PRIVATE
    ${MODEL_DEFINE}
)

#==============================================================================
# 6. Subdirectories
#==============================================================================

add_subdirectory(examples)
add_subdirectory(test/unit)

#==============================================================================
# 7. FMU packaging
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/models/${MODEL}/modelDescription.xml
        ${FMU_OUTPUT_DIR}
        ${LIB_NAME}
    COMMENT "Packaging FMU to ${FMU_OUTPUT_DIR}/${LIB_NAME}.fmu ..."
    VERBATIM
)