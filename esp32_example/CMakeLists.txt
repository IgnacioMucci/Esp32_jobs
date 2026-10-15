cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common utils handlers)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options(-fdiagnostics-color=always)

project(esp32_example)