cmake_minimum_required(VERSION 3.12)
project(fmuTemplate VERSION 1.0.0)

include(CTest)
enable_testing()

set(LIB_NAME fmuTemplate)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform-specific settings for FMU
if(WIN32)
    set(FMI_PLATFORM "x86_64-windows")
elseif(APPLE)
    set(FMI_PLATFORM "x86_64-darwin")
else()
    set(FMI_PLATFORM "x86_64-linux")
endif()

# Build the FMU as a shared library (DLL/SO)
add_library(${LIB_NAME} SHARED
    src/model.c
    src/fmi2Adapter.c
)

# Set the output name to match FMI3 requirements
set_target_properties(${LIB_NAME} PROPERTIES
    PREFIX ""  # Remove "lib" prefix on Unix
    OUTPUT_NAME "${LIB_NAME}"
    POSITION_INDEPENDENT_CODE ON
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(${LIB_NAME} PRIVATE
        FMI3_FUNCTION_PREFIX=${LIB_NAME}_
        _CRT_SECURE_NO_WARNINGS
    )
    set_target_properties(${LIB_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

# Define FMI3_FUNCTION_PREFIX for all platforms
target_compile_definitions(${LIB_NAME} PRIVATE
    FMI3_FUNCTION_PREFIX=${LIB_NAME}_
)

target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fmi2/src
)

# Optional: Keep static library for testing
add_library(${LIB_NAME}_static STATIC
    src/model.c
    src/fmi2Adapter.c
)

target_include_directories(${LIB_NAME}_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fmi2/src
)

# add_subdirectory(test)
add_subdirectory(examples)

# Install rules for FMU packaging
install(TARGETS ${LIB_NAME}
    LIBRARY DESTINATION binaries/${FMI_PLATFORM}
    RUNTIME DESTINATION binaries/${FMI_PLATFORM}
)

# FMU Packaging
# Check for virtual environment Python first
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

add_custom_command(
    TARGET ${LIB_NAME} POST_BUILD
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_fmu.py
        $<TARGET_FILE:${LIB_NAME}>
        ${CMAKE_CURRENT_SOURCE_DIR}/config/modelDescription.xml
        ${CMAKE_BINARY_DIR}
        ${LIB_NAME}
    COMMENT "Packaging FMU..."
    VERBATIM
)