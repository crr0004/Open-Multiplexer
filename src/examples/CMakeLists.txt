add_executable(echocon ${CMAKE_CURRENT_SOURCE_DIR}/microsoft/echocon.cpp)
target_compile_options(echocon PRIVATE "-w" -g -O0 -gdwarf)

add_executable(ms_example ${CMAKE_CURRENT_SOURCE_DIR}/microsoft/ms_example.cpp)
target_compile_options(ms_example PRIVATE "-w")