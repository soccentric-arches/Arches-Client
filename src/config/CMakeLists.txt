add_library(arches_config STATIC
    client_config.cpp
    client_id.cpp
)

target_include_directories(arches_config PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(arches_config PRIVATE
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    yaml-cpp::yaml-cpp
)