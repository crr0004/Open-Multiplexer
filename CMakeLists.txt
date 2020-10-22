cmake_minimum_required(VERSION 3.18)
project("OpenMultiplexer")
SET(SHORT_NAME omux)

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/omux/omux.cpp
)

add_executable(${SHORT_NAME} ${SOURCE_FILES})
target_compile_definitions(${SHORT_NAME} PRIVATE UNICODE PRIVATE _UNICODE)
set_property(TARGET ${SHORT_NAME} PROPERTY CXX_STANDARD 20)


add_executable(echocon ${CMAKE_SOURCE_DIR}/src/examples/microsoft/echocon.cpp)
target_compile_options(echocon PRIVATE "-w")

add_executable(ms_example ${CMAKE_SOURCE_DIR}/src/examples/microsoft/ms_example.cpp)
target_compile_options(ms_example PRIVATE "-w")