
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
target_compile_definitions(test_newModel PRIVATE ${MODEL_DEFINE})
target_link_libraries(test_newModel PRIVATE ${LIB_NAME}_static unity)
target_include_directories(test_newModel PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/unity/src 
)

#==============================================================================
# 3. Register tests
#==============================================================================

add_test(NAME test_newModel COMMAND test_newModel)