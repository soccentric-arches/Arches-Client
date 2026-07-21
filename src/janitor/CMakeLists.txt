add_library(acadia_janitor STATIC
    Janitor.cc
)

target_include_directories(acadia_janitor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(acadia_janitor PRIVATE
    acadia_protos
    spdlog::spdlog
)
