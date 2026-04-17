cmake_minimum_required(VERSION 3.16)
project(lwipc VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LWIPC_ENABLE_PROTOBUF "Enable protobuf codec support" ON)

add_library(lwipc STATIC
    src/broker.cpp
    src/executor.cpp
    src/heartbeat.cpp
    src/memory_pool.cpp
    src/shm_segment.cpp
    src/topology.cpp
    src/transport.cpp
)

target_include_directories(lwipc PUBLIC include)
target_compile_options(lwipc PRIVATE -Wall -Wextra -Wpedantic)

if(LWIPC_ENABLE_PROTOBUF)
  find_package(Protobuf QUIET)
  if(Protobuf_FOUND)
    protobuf_generate_cpp(LWIPC_PROTO_SRCS LWIPC_PROTO_HDRS proto/lwipc_frame.proto)
    target_sources(lwipc PRIVATE src/proto_codec.cpp ${LWIPC_PROTO_SRCS} ${LWIPC_PROTO_HDRS})
    target_include_directories(lwipc PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(lwipc PUBLIC protobuf::libprotobuf)
    target_compile_definitions(lwipc PUBLIC LWIPC_WITH_PROTOBUF)
    message(STATUS "Protobuf support enabled")
  else()
    message(WARNING "Protobuf not found. Build without protobuf codec support.")
  endif()
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(test_ringbuffer tests/test_ringbuffer.cpp)
  target_link_libraries(test_ringbuffer PRIVATE lwipc)
  add_test(NAME ringbuffer COMMAND test_ringbuffer)

  add_executable(test_broker tests/test_broker.cpp)
  target_link_libraries(test_broker PRIVATE lwipc)
  add_test(NAME broker COMMAND test_broker)

  add_executable(test_shm tests/test_shm.cpp)
  target_link_libraries(test_shm PRIVATE lwipc)
  add_test(NAME shm COMMAND test_shm)

  add_executable(test_memory_pool tests/test_memory_pool.cpp)
  target_link_libraries(test_memory_pool PRIVATE lwipc)
  add_test(NAME memory_pool COMMAND test_memory_pool)

  add_executable(test_metrics tests/test_metrics.cpp)
  target_link_libraries(test_metrics PRIVATE lwipc)
  add_test(NAME metrics COMMAND test_metrics)

  add_executable(test_executor tests/test_executor.cpp)
  target_link_libraries(test_executor PRIVATE lwipc)
  add_test(NAME executor COMMAND test_executor)

  add_executable(test_topology tests/test_topology.cpp)
  target_link_libraries(test_topology PRIVATE lwipc)
  add_test(NAME topology COMMAND test_topology)

  add_executable(test_heartbeat tests/test_heartbeat.cpp)
  target_link_libraries(test_heartbeat PRIVATE lwipc)
  add_test(NAME heartbeat COMMAND test_heartbeat)

  add_executable(test_transport tests/test_transport.cpp)
  target_link_libraries(test_transport PRIVATE lwipc)
  add_test(NAME transport COMMAND test_transport)

  add_executable(test_core tests/test_core.cpp)
  target_link_libraries(test_core PRIVATE lwipc)
  add_test(NAME core COMMAND test_core)

  if(TARGET protobuf::libprotobuf)
    add_executable(test_proto_codec tests/test_proto_codec.cpp)
    target_link_libraries(test_proto_codec PRIVATE lwipc)
    add_test(NAME proto_codec COMMAND test_proto_codec)
  endif()
endif()
