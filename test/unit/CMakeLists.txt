
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
target_link_libraries(test_eulerForward PRIVATE model_core unity m)
target_include_directories(test_eulerForward PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/unity/src
)

# rungeKutta4
add_executable(test_rungeKutta4 test_rungeKutta4.c)
target_link_libraries(test_rungeKutta4 PRIVATE model_core unity m)
target_include_directories(test_rungeKutta4 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/unity/src
)

#==============================================================================
# 3. Register tests
#==============================================================================

add_test(NAME test_newModel COMMAND test_newModel)
add_test(NAME test_freeModel COMMAND test_freeModel)
add_test(NAME test_modelData COMMAND test_modelData)
add_test(NAME test_eulerForward COMMAND test_eulerForward)
add_test(NAME test_rungeKutta4 COMMAND test_rungeKutta4)