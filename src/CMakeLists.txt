# ---------------------------------------------------------------------------
# src/CMakeLists.txt
# Adds each subsystem as a static library then defines the main executable.
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

add_executable(arches-client
    main.cpp
    app.cpp
)

target_include_directories(arches-client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../Arches-HAL/src
)

# gRPC runtime + protobuf are carried transitively by hal_protos_shared.
target_link_libraries(arches-client PRIVATE
    arches_config
    arches_diag
    arches_hal_bridge
    arches_hal_service
    arches_janitor
    arches_log
    arches_ota
    arches_registry
    arches_run
    arches_spool
    arches_stream
    hal_protos_shared
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    yaml-cpp::yaml-cpp
    SQLite::SQLite3
)

target_compile_definitions(arches-client PRIVATE
    ARCHES_CLIENT_VERSION="${PROJECT_VERSION}"
)

set_compiler_warnings(arches-client)