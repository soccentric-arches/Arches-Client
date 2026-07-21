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
    ${CMAKE_SOURCE_DIR}/../Arches-HAL/src
)

target_link_libraries(test_result_serializer PRIVATE
    arches_hal_bridge
    hal_protos_shared
    spdlog::spdlog
    GTest::gtest_main
)

add_test(NAME serializer_tests COMMAND test_result_serializer)
set_tests_properties(serializer_tests PROPERTIES LABELS "arches_client")

# ---------------------------------------------------------------------------
# test_run_orchestrator — integration tests for RunOrchestrator
# ---------------------------------------------------------------------------

add_executable(test_run_orchestrator
    test_run_orchestrator.cc
)

target_include_directories(test_run_orchestrator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Arches-HAL/src
)

# gRPC runtime + protobuf come transitively from hal_protos_shared.
target_link_libraries(test_run_orchestrator PRIVATE
    arches_hal_bridge
    arches_registry
    arches_spool
    arches_stream
    arches_run
    hal_protos_shared
    spdlog::spdlog
    SQLite::SQLite3
    yaml-cpp::yaml-cpp
    GTest::gtest_main
)

add_test(NAME runner_tests COMMAND test_run_orchestrator)
set_tests_properties(runner_tests PROPERTIES LABELS "arches_client")

# ---------------------------------------------------------------------------
# test_adapter_registry — registry dispatch + honest-stub behavior
# ---------------------------------------------------------------------------

add_executable(test_adapter_registry
    test_adapter_registry.cc
)

target_include_directories(test_adapter_registry PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Arches-HAL/src
)

target_link_libraries(test_adapter_registry PRIVATE
    arches_registry
    hal_protos_shared
    spdlog::spdlog
    GTest::gtest_main
)

add_test(NAME adapter_registry_tests COMMAND test_adapter_registry)
set_tests_properties(adapter_registry_tests PROPERTIES LABELS "arches_client")

# ---------------------------------------------------------------------------
# test_app_wiring — build_adapter_registry covers proto-only modules w/ stub
# ---------------------------------------------------------------------------

add_executable(test_app_wiring
    test_app_wiring.cc
)

target_include_directories(test_app_wiring PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Arches-HAL/src
)

target_link_libraries(test_app_wiring PRIVATE
    arches_registry
    arches_hal_bridge
    hal_protos_shared
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    GTest::gtest_main
)

add_test(NAME app_wiring_tests COMMAND test_app_wiring)
set_tests_properties(app_wiring_tests PROPERTIES LABELS "arches_client")