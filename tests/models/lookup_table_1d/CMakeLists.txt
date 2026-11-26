#==============================================================================
# C tests of the lookup_table_1d model (built only with -DMODEL=lookup_table_1d)
#==============================================================================

# Reading the table file (the parser in equations.c) through the model layer,
# called without FMPy so that the sanitizer job covers it. The test reads the
# packaged table and writes its own tables into its build folder
add_executable(test_table_parser test_table_parser.c)
target_link_libraries(test_table_parser PRIVATE ${LIB_NAME}_static unity)
target_compile_definitions(test_table_parser PRIVATE
    PACKAGED_RESOURCES="${CMAKE_SOURCE_DIR}/src/models/lookup_table_1d/resources/"
    TABLE_DIR="${CMAKE_CURRENT_BINARY_DIR}/"
)

add_test(NAME test_table_parser COMMAND test_table_parser)
