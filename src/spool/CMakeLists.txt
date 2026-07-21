add_library(acadia_spool STATIC
    Spool.cc
)

target_include_directories(acadia_spool PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(acadia_spool PRIVATE
    acadia_protos
    SQLite::SQLite3
    spdlog::spdlog
)
