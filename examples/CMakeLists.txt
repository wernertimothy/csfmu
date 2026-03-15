
#==============================================================================
# Model-specific C examples
# Each example is self-contained and only builds for its target model.
#==============================================================================

if(MODEL STREQUAL "first_order")
    add_executable(example_model example_model.c)
    target_compile_definitions(example_model PRIVATE ${MODEL_DEFINE})
    target_link_libraries(example_model PRIVATE ${LIB_NAME}_static)

    add_executable(example_fmu example_fmu.c)
    target_compile_definitions(example_fmu PRIVATE ${MODEL_DEFINE})
    target_link_libraries(example_fmu PRIVATE ${LIB_NAME}_static)
endif()