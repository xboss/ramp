cmake_minimum_required(VERSION 3.20.0)
project(ramp VERSION 0.1.0)

SET(CMAKE_C_FLAGS_DEBUG "$ENV{CXXFLAGS} -std=c89 -O0 -Wno-deprecated-declarations -DDEBUG -ggdb -fsanitize=leak -fsanitize=address")
# SET(CMAKE_C_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb ")
SET(CMAKE_C_FLAGS_RELEASE "$ENV{CXXFLAGS} -std=c89 -O3 -Wall")

message("project dir: ${PROJECT_SOURCE_DIR} compiler: ${CMAKE_C_COMPILER_ID} platform: ${CMAKE_SYSTEM_NAME}")

include_directories(${PROJECT_SOURCE_DIR}/src/)
aux_source_directory(${PROJECT_SOURCE_DIR}/src DIR_SRCS)

############# ramp #############
add_executable(ramp ${DIR_SRCS})
target_link_libraries(ramp ssl)
target_link_libraries(ramp crypto)
target_link_libraries(ramp pthread)

#cmake -DCMAKE_BUILD_TYPE=Debug ..
