cmake_minimum_required(VERSION 3.19)
project("OpenMultiplexer" CXX)
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

SET(INCLUDE_FILES 
    ${CMAKE_SOURCE_DIR}/src/apis/alias.hpp
    ${CMAKE_SOURCE_DIR}/src/omux/console.hpp
)

SET(REQUIRE_COMPILE_FEATURES 
    cxx_auto_type
    cxx_generalized_initializers
    cxx_defaulted_move_initializers
    cxx_deleted_functions
    cxx_lambdas
    cxx_long_long_type
    cxx_noexcept
    cxx_nullptr
    cxx_range_for
    cxx_rvalue_references
    cxx_sizeof_member
    cxx_generic_lambdas
    cxx_return_type_deduction
)

set(build_flags "-Wall" "-pedantic" "-Wno-c++98-compat" "-Wno-c++98-compat-pedantic")

OPTION(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${SHORT_NAME} ${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/src/omux/omux.cpp)
target_compile_definitions(${SHORT_NAME} PRIVATE UNICODE PRIVATE _UNICODE)
set_target_properties(${SHORT_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_options(${SHORT_NAME} PRIVATE ${build_flags})
target_include_directories(${SHORT_NAME} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(${SHORT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/)
target_compile_features(${SHORT_NAME} PRIVATE ${REQUIRE_COMPILE_FEATURES})

add_executable(${SHORT_NAME}_test ${TEST_SOURCE_FILES} ${SOURCE_FILES})
target_compile_definitions(${SHORT_NAME}_test PRIVATE UNICODE PRIVATE _UNICODE)
set_target_properties(${SHORT_NAME}_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(${SHORT_NAME}_test SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/lib/include)
target_include_directories(${SHORT_NAME}_test PRIVATE ${CMAKE_SOURCE_DIR}/src/)
target_compile_options(${SHORT_NAME}_test PRIVATE ${build_flags})
target_compile_features(${SHORT_NAME}_test PRIVATE ${REQUIRE_COMPILE_FEATURES})

add_subdirectory(${CMAKE_SOURCE_DIR}/src/examples/)


#set_property(TARGET tidy_check PROPERTY CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)

add_custom_target(tidy_check 
    clang-tidy -checks=-*,readability-*,cppcoreguidelines-*,concurrency-*,clang-analyzer-*,modernize-*
    ${SOURCE_FILES} ${TEST_FILES} ${CMAKE_SOURCE_DIR}/src/omux/omux.cpp ${INCLUDE_FILES}
    -p ${CMAKE_CURRENT_BINARY_DIR}
    -fix
    )