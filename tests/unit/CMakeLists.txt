#==============================================================================
# 1. Unity static lib
#==============================================================================

add_library(unity STATIC ${CMAKE_SOURCE_DIR}/external/unity/src/unity.c)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
target_include_directories(unity PUBLIC ${CMAKE_SOURCE_DIR}/external/unity/src)

#==============================================================================
# 2. Test executables
#==============================================================================

# Builds <name>.c (plus further sources given after the name) against
# model_core and Unity, and registers it with CTest
function(csfmu_unit_test name)
    add_executable(${name} ${name}.c ${ARGN})
    target_link_libraries(${name} PRIVATE model_core unity)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

csfmu_unit_test(test_newModel)
csfmu_unit_test(test_freeModel)
csfmu_unit_test(test_modelData)  # saveModelData / restoreModelData
csfmu_unit_test(test_initialize)  # initializeModel / computeCalculatedParameters / modelError
csfmu_unit_test(test_enumeration)  # modelEnumerationHasValue / enumeration variables
csfmu_unit_test(test_string)  # string parameters / modelSetString
csfmu_unit_test(test_resources)  # modelSetResourcePath / modelOpenFile
csfmu_unit_test(test_eulerForward)
csfmu_unit_test(test_rungeKutta4)
csfmu_unit_test(test_simulation)  # simulateTo / modelDiscard / model-layer result checks

# Integrator time contract, checked by fmi2DoStep. The FMI wrapper is linked
# against a model definition of the test itself (instead of the selected
# model), so the test is built for every model
csfmu_unit_test(test_fmi2_integratorContract ${CMAKE_SOURCE_DIR}/src/fmi2Functions.c)
target_include_directories(test_fmi2_integratorContract PRIVATE ${CMAKE_SOURCE_DIR}/external/fmi2/src)

# Internals block in the FMU state, the serialized FMU state, fmi2Reset and
# the rollback of a discarded step, with a model definition of the test itself
csfmu_unit_test(test_fmi2_internals ${CMAKE_SOURCE_DIR}/src/fmi2Functions.c)
target_include_directories(test_fmi2_internals PRIVATE ${CMAKE_SOURCE_DIR}/external/fmi2/src)

# Local variables: read with every getter, never set, follow a set without a
# step, and a non-finite Real local discards the step
csfmu_unit_test(test_fmi2_locals ${CMAKE_SOURCE_DIR}/src/fmi2Functions.c)
target_include_directories(test_fmi2_locals PRIVATE ${CMAKE_SOURCE_DIR}/external/fmi2/src)

# Initialization: calculated parameters in every state, the model's
# initialize function and its errors, states with initial="approx"
csfmu_unit_test(test_fmi2_initialize ${CMAKE_SOURCE_DIR}/src/fmi2Functions.c)
target_include_directories(test_fmi2_initialize PRIVATE ${CMAKE_SOURCE_DIR}/external/fmi2/src)

# Enumeration variables: Integer access, only item values can be set
csfmu_unit_test(test_fmi2_enumeration ${CMAKE_SOURCE_DIR}/src/fmi2Functions.c)
target_include_directories(test_fmi2_enumeration PRIVATE ${CMAKE_SOURCE_DIR}/external/fmi2/src)

# String parameters: get, set (copied in, refused if too long or NULL), fixed
# and tunable, part of the FMU state and of fmi2Reset
csfmu_unit_test(test_fmi2_string ${CMAKE_SOURCE_DIR}/src/fmi2Functions.c)
target_include_directories(test_fmi2_string PRIVATE ${CMAKE_SOURCE_DIR}/external/fmi2/src)

# Resource folder: fmuResourceLocation (file URI) becomes model->resourcePath,
# other locations leave the model without one, never failing the instantiation
csfmu_unit_test(test_fmi2_resources ${CMAKE_SOURCE_DIR}/src/fmi2Functions.c)
target_include_directories(test_fmi2_resources PRIVATE ${CMAKE_SOURCE_DIR}/external/fmi2/src)

# Checks of the model definition in the modelDescription.xml generator. The
# test includes src/fmi2ModelDescription.c to reach its static functions
csfmu_unit_test(test_validateDefinition)

# The DefaultExperiment element of the generator for experiments that suggest
# only some settings, or none (the golden test below covers all four)
csfmu_unit_test(test_defaultExperiment)

#==============================================================================
# 3. modelDescription.xml generator
#==============================================================================

# The generator, linked against a model definition that uses every feature of
# the generator (instead of a real model)
add_executable(generate_fixture_description
    ${CMAKE_SOURCE_DIR}/src/fmi2ModelDescription.c
    fixtures/generator_model.c
)
# model_core provides the model-layer helpers the generator uses (e.g. modelEnumerationHasValue)
target_link_libraries(generate_fixture_description PRIVATE model_core)

# The generated modelDescription.xml must be equal to the handwritten one
add_test(NAME test_modelDescription_golden
    COMMAND ${CMAKE_COMMAND}
        -DGENERATOR=$<TARGET_FILE:generate_fixture_description>
        -DIDENTIFIER=generator_model
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/generator_model.xml
        -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generator_model.expected.xml
        -P ${CMAKE_CURRENT_SOURCE_DIR}/compare_generated.cmake
)

# A problem in the model definition makes the generator fail, which stops the
# build. The individual checks are tested by test_validateDefinition
add_test(NAME test_modelDescription_rejectsWrongIdentifier
    COMMAND generate_fixture_description ${CMAKE_CURRENT_BINARY_DIR}/rejected.xml another_name
)
set_tests_properties(test_modelDescription_rejectsWrongIdentifier PROPERTIES WILL_FAIL TRUE)
