add_library(arches_log STATIC
    LogCollector.cc
)

target_include_directories(arches_log PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(arches_log PRIVATE
    hal_protos_shared
    spdlog::spdlog
)