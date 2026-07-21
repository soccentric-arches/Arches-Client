add_library(arches_spool STATIC
    Spool.cc
)

target_include_directories(arches_spool PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(arches_spool PRIVATE
    hal_protos_shared
    SQLite::SQLite3
    spdlog::spdlog
)