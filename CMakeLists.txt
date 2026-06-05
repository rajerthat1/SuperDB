cmake_minimum_required(VERSION 3.20)
project(key_val)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kv-server
  src/main.cpp
  src/server/server.cpp
  src/server/connection.cpp
  src/protocol/resp.cpp
  src/storage/in_memory_storage.cpp
  src/commands/handler.cpp
)

target_include_directories(kv-server PRIVATE src)
target_compile_options(kv-server PRIVATE -O2 -Wall -Wextra -g)
target_link_libraries(kv-server pthread)
