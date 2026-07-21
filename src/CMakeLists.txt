# ---------------------------------------------------------------------------
# src/CMakeLists.txt
# Adds each module as a static library then defines the main executable.
# Subdirectories are ordered leaf-first so inter-target references resolve.
# ---------------------------------------------------------------------------

add_subdirectory(config)
add_subdirectory(diag)
add_subdirectory(spool)
add_subdirectory(stream)
add_subdirectory(hal_bridge)
add_subdirectory(registry)
add_subdirectory(ota)
add_subdirectory(log)
add_subdirectory(janitor)
add_subdirectory(run)
add_subdirectory(hal_service)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------

add_executable(acadia-client
    main.cpp
    app.cpp
)

target_include_directories(acadia-client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../Acadia-HAL/src
)

target_link_libraries(acadia-client PRIVATE
    acadia_config
    acadia_diag
    acadia_hal_bridge
    acadia_hal_service
    acadia_janitor
    acadia_log
    acadia_ota
    acadia_registry
    acadia_run
    acadia_spool
    acadia_stream
    acadia_protos
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    yaml-cpp::yaml-cpp
    SQLite::SQLite3
    ${ACADIA_GRPC_LIBRARIES}
    protobuf::libprotobuf
)

target_compile_definitions(acadia-client PRIVATE
    ACADIA_CLIENT_VERSION="${PROJECT_VERSION}"
)

set_compiler_warnings(acadia-client)
