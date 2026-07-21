add_library(arches_hal_bridge STATIC
    ResultSerializer.cc
)

target_include_directories(arches_hal_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Arches-HAL/src
)

target_link_libraries(arches_hal_bridge PRIVATE
    hal_protos_shared
    spdlog::spdlog
)