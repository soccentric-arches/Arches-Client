add_library(acadia_log STATIC
    LogCollector.cc
)

target_include_directories(acadia_log PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(acadia_log PRIVATE
    acadia_protos
    spdlog::spdlog
)
