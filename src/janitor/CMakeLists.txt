add_library(arches_janitor STATIC
    Janitor.cc
)

target_include_directories(arches_janitor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(arches_janitor PRIVATE
    hal_protos_shared
    spdlog::spdlog
)