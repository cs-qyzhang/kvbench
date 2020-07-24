cmake_minimum_required(VERSION 3.3)
project(kvbench)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")

include_directories(${PROJECT_SOURCE_DIR}/include)

set(KVBENCH_LIB protobuf pthread)

add_executable(std_map_bench db/std_map.cc)
target_link_libraries(std_map_bench ${KVBENCH_LIB})