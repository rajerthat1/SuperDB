include(FetchContent)
cmake_policy(SET CMP0135 NEW)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(test_resp
  test_resp.cpp
  ${CMAKE_SOURCE_DIR}/src/protocol/resp.cpp
)
target_include_directories(test_resp PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_resp GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_resp)

add_executable(test_storage
  test_storage.cpp
  ${CMAKE_SOURCE_DIR}/src/storage/in_memory_storage.cpp
  ${CMAKE_SOURCE_DIR}/src/storage/wal_engine.cpp
  ${CMAKE_SOURCE_DIR}/src/protocol/resp.cpp
)
target_include_directories(test_storage PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_storage GTest::gtest GTest::gtest_main pthread)
gtest_discover_tests(test_storage)
