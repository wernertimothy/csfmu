
#==============================================================================
# example_model.c
#==============================================================================

add_executable(example_model example_model.c)
target_compile_definitions(example_model PRIVATE ${MODEL_DEFINE})
target_link_libraries(example_model PRIVATE ${LIB_NAME}_static)

#==============================================================================
# example_fmu.c
#==============================================================================

add_executable(example_fmu example_fmu.c)
target_compile_definitions(example_fmu PRIVATE ${MODEL_DEFINE})
target_link_libraries(example_fmu PRIVATE ${LIB_NAME}_static)