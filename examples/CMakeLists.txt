
add_executable(example_model example_model.c)
add_executable(example_fmu example_fmu.c)

target_link_libraries(example_model PRIVATE ${LIB_NAME}_static)
target_link_libraries(example_fmu PRIVATE ${LIB_NAME}_static)