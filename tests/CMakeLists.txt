# ---------------------------------------------------------------------------
# tests/CMakeLists.txt
# ---------------------------------------------------------------------------

# GTest
find_package(GTest REQUIRED)

# ---------------------------------------------------------------------------
# test_result_serializer — unit tests for hal_bridge serialization
# ---------------------------------------------------------------------------

add_executable(test_result_serializer
    test_result_serializer.cc
)

target_include_directories(test_result_serializer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Acadia-HAL/src
)

target_link_libraries(test_result_serializer PRIVATE
    acadia_hal_bridge
    acadia_protos
    spdlog::spdlog
    GTest::gtest_main
)

add_test(NAME serializer_tests COMMAND test_result_serializer)

# ---------------------------------------------------------------------------
# test_run_orchestrator — integration tests for RunOrchestrator
# ---------------------------------------------------------------------------

add_executable(test_run_orchestrator
    test_run_orchestrator.cc
)

target_include_directories(test_run_orchestrator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Acadia-HAL/src
)

target_link_libraries(test_run_orchestrator PRIVATE
    acadia_hal_bridge
    acadia_registry
    acadia_spool
    acadia_stream
    acadia_run
    acadia_protos
    spdlog::spdlog
    SQLite::SQLite3
    yaml-cpp::yaml-cpp
    ${ACADIA_GRPC_LIBRARIES}
    protobuf::libprotobuf
    GTest::gtest_main
)

add_test(NAME runner_tests COMMAND test_run_orchestrator)
