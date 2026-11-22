#==============================================================================
# C tests of the fault_injection model (built only with -DMODEL=fault_injection)
#==============================================================================

# Discard and rollback path of fmi2DoStep, called without FMPy so that the
# sanitizer job covers it
add_executable(test_fmi2_discard test_fmi2_discard.c)
target_link_libraries(test_fmi2_discard PRIVATE ${LIB_NAME}_static unity)
target_include_directories(test_fmi2_discard PRIVATE ${CMAKE_SOURCE_DIR}/external/unity/src)

add_test(NAME test_fmi2_discard COMMAND test_fmi2_discard)
