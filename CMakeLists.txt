cmake_minimum_required(VERSION 3.1)
project("OpenMultiplexer")
SET(SHORT_NAME omux)

SET(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/omux/console.cpp
    ${CMAKE_SOURCE_DIR}/src/omux/process.cpp
    ${CMAKE_SOURCE_DIR}/src/omux/primary_console.cpp
    ${CMAKE_SOURCE_DIR}/src/apis/windows.cpp
    ${CMAKE_SOURCE_DIR}/src/apis/primary_console.cpp
)

SET(TEST_SOURCE_FILES 
    ${CMAKE_SOURCE_DIR}/src/test/omux.cpp
    ${CMAKE_SOURCE_DIR}/src/test/windows_apis.cpp
)

add_executable(${SHORT_NAME} ${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/src/omux/omux.cpp)
target_compile_definitions(${SHORT_NAME} PRIVATE UNICODE PRIVATE _UNICODE)
set_property(TARGET ${SHORT_NAME} PROPERTY CXX_STANDARD 20)
target_compile_options(${SHORT_NAME} PRIVATE "-Wall" "-pedantic")
target_include_directories(${SHORT_NAME} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(${SHORT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/)

add_executable(${SHORT_NAME}_test ${TEST_SOURCE_FILES} ${SOURCE_FILES})
target_compile_definitions(${SHORT_NAME}_test PRIVATE UNICODE PRIVATE _UNICODE)
set_property(TARGET ${SHORT_NAME}_test PROPERTY CXX_STANDARD 20)
target_include_directories(${SHORT_NAME}_test SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(${SHORT_NAME}_test PRIVATE ${CMAKE_SOURCE_DIR}/src/)
target_compile_options(${SHORT_NAME}_test PRIVATE "-Wall" "-pedantic")

add_subdirectory(${CMAKE_SOURCE_DIR}/src/examples/)