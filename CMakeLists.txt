cmake_minimum_required(VERSION 3.20)

project(emlisp VERSION 1.0 LANGUAGES CXX)

include_directories(inc/)

add_library(emlisp inc/emlisp.h src/emlisp.cpp)
target_compile_features(emlisp PUBLIC cxx_std_17)
export(TARGETS emlisp FILE EmlispTargets.cmake)

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
