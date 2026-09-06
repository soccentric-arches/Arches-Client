add_library(arches_run STATIC
    RunOrchestrator.cc
)

target_include_directories(arches_run PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Arches-HAL/src
)

target_link_libraries(arches_run PRIVATE
    arches_instrumentation
    arches_hal_bridge
    hal_protos_shared
    spdlog::spdlog
)