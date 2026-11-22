# Runs the modelDescription.xml generator and compares its result with the
# expected file.
#
# Usage: cmake -DGENERATOR=<executable> -DIDENTIFIER=<model identifier>
#              -DOUTPUT=<generated file> -DEXPECTED=<expected file>
#              -P compare_generated.cmake

execute_process(
    COMMAND ${GENERATOR} ${OUTPUT} ${IDENTIFIER}
    RESULT_VARIABLE generator_result
)
if(NOT generator_result EQUAL 0)
    message(FATAL_ERROR "The generator failed (${generator_result})")
endif()

# Line endings are ignored, they differ between platforms
execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${OUTPUT} ${EXPECTED}
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    file(READ ${OUTPUT} generated)
    message(FATAL_ERROR "${OUTPUT} differs from ${EXPECTED}. Generated:\n${generated}")
endif()
