cmake_minimum_required(VERSION 3.20)

project(emlisp VERSION 1.0 LANGUAGES CXX)

include_directories(inc/)

add_executable(emlisp_src_embed_gen src/src_embed_gen.cpp)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lisp_std.cpp
    COMMAND emlisp_src_embed_gen ${CMAKE_CURRENT_BINARY_DIR}/lisp_std.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/lisp_std_template.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/std.lisp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/lisp_std_template.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/std.lisp
)

add_library(emlisp inc/emlisp.h src/memory.cpp src/reader.cpp src/eval.cpp src/funcs.cpp lisp_std.cpp)
target_compile_features(emlisp PUBLIC cxx_std_17)
export(TARGETS emlisp FILE EmlispTargets.cmake)

add_executable(emlisp_repl tests/repl.cpp)
target_link_libraries(emlisp_repl emlisp)
target_compile_features(emlisp_repl PUBLIC cxx_std_17)

enable_testing()

function(add_test_suite TEST_INOUT_PATH TEST_DRIVER)
    file(GLOB test_inputs LIST_DIRECTORIES false tests/${TEST_INOUT_PATH}/in*.txt)
    file(GLOB test_outputs LIST_DIRECTORIES false tests/${TEST_INOUT_PATH}/out*.txt)
    message("Adding tests for ${TEST_INOUT_PATH} using ${TEST_DRIVER}, inputs = ${test_inputs}, outputs = ${test_outputs}")
    foreach(test IN ZIP_LISTS test_inputs test_outputs)
        add_test(NAME ${test_0} COMMAND sh -c "${CMAKE_BINARY_DIR}/${TEST_DRIVER} ${test_0} | diff - ${test_1} --color -w")
    endforeach()
endfunction()

add_executable(test_parser_driver tests/parser/driver.cpp)
target_link_libraries(test_parser_driver emlisp)
add_test_suite(parser test_parser_driver)

add_executable(test_eval_driver tests/eval/driver.cpp)
target_link_libraries(test_eval_driver emlisp)
file(GLOB test_inputs LIST_DIRECTORIES false tests/eval/*.lisp)
message("eval tests: ${test_inputs}")
foreach(test ${test_inputs})
	add_test(NAME ${test} COMMAND test_eval_driver ${test})
endforeach()
add_test(NAME test-stdlib COMMAND test_eval_driver ${CMAKE_CURRENT_SOURCE_DIR}/tests/std.lisp --include-stdlib)
