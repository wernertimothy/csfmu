
#==============================================================================
# 1. Unity static lib
#==============================================================================

add_library(unity STATIC ${CMAKE_SOURCE_DIR}/external/unity/src/unity.c)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

#==============================================================================
# 2. Test executables
#==============================================================================

# newModel
add_executable(test_newModel test_newModel.c)
target_link_libraries(test_newModel PRIVATE model_core unity)
target_include_directories(test_newModel PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/unity/src 
)

# freeModel
add_executable(test_freeModel test_freeModel.c)
target_link_libraries(test_freeModel PRIVATE model_core unity)
target_include_directories(test_freeModel PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/unity/src 
)

# saveModelData / restoreModelData
add_executable(test_modelData test_modelData.c)
target_link_libraries(test_modelData PRIVATE model_core unity)
target_include_directories(test_modelData PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/unity/src
)

# eulerForward
add_executable(test_eulerForward test_eulerForward.c)
target_link_libraries(test_eulerForward PRIVATE model_core unity)
target_include_directories(test_eulerForward PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/unity/src
)

# rungeKutta4
add_executable(test_rungeKutta4 test_rungeKutta4.c)
target_link_libraries(test_rungeKutta4 PRIVATE model_core unity)
target_include_directories(test_rungeKutta4 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/unity/src
)

# Integrator time contract, checked by fmi2DoStep. The FMI wrapper is linked
# against a model definition of the test itself (instead of the selected
# model), so the test is built for every model
add_executable(test_fmi2_integratorContract
    test_fmi2_integratorContract.c
    ${CMAKE_SOURCE_DIR}/src/fmi2Functions.c
)
target_link_libraries(test_fmi2_integratorContract PRIVATE model_core unity)
target_include_directories(test_fmi2_integratorContract PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/fmi2/src
    ${CMAKE_SOURCE_DIR}/external/unity/src
)

# Math library (not part of the C library on Linux, does not exist for MSVC)
if(UNIX)
    target_link_libraries(test_eulerForward PRIVATE m)
    target_link_libraries(test_rungeKutta4 PRIVATE m)
    target_link_libraries(test_fmi2_integratorContract PRIVATE m)
endif()

# modelDescription.xml generator, linked against a model definition that uses
# every feature of the generator (instead of a real model)
add_executable(generate_fixture_description
    ${CMAKE_SOURCE_DIR}/src/fmi2ModelDescription.c
    fixtures/generator_model.c
)
target_include_directories(generate_fixture_description PRIVATE ${CMAKE_SOURCE_DIR}/include)

# The same definition with an output that depends on an unknown input
add_executable(generate_invalid_description
    ${CMAKE_SOURCE_DIR}/src/fmi2ModelDescription.c
    fixtures/generator_model.c
)
target_include_directories(generate_invalid_description PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(generate_invalid_description PRIVATE INVALID_DEPENDENCY)

#==============================================================================
# 3. Register tests
#==============================================================================

add_test(NAME test_newModel COMMAND test_newModel)
add_test(NAME test_freeModel COMMAND test_freeModel)
add_test(NAME test_modelData COMMAND test_modelData)
add_test(NAME test_eulerForward COMMAND test_eulerForward)
add_test(NAME test_rungeKutta4 COMMAND test_rungeKutta4)
add_test(NAME test_fmi2_integratorContract COMMAND test_fmi2_integratorContract)

# The generated modelDescription.xml must be equal to the handwritten one
add_test(NAME test_modelDescription_golden
    COMMAND ${CMAKE_COMMAND}
        -DGENERATOR=$<TARGET_FILE:generate_fixture_description>
        -DIDENTIFIER=generator_model
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/generator_model.xml
        -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generator_model.expected.xml
        -P ${CMAKE_CURRENT_SOURCE_DIR}/compare_generated.cmake
)

# The generator must reject mistakes in the model definition
add_test(NAME test_modelDescription_rejectsWrongIdentifier
    COMMAND generate_fixture_description ${CMAKE_CURRENT_BINARY_DIR}/rejected.xml another_name
)
add_test(NAME test_modelDescription_rejectsUnknownDependency
    COMMAND generate_invalid_description ${CMAKE_CURRENT_BINARY_DIR}/rejected.xml generator_model
)
set_tests_properties(
    test_modelDescription_rejectsWrongIdentifier
    test_modelDescription_rejectsUnknownDependency
    PROPERTIES WILL_FAIL TRUE
)
