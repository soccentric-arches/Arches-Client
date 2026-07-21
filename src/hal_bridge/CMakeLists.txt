add_library(acadia_hal_bridge STATIC
    ResultSerializer.cc
)

target_include_directories(acadia_hal_bridge PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Acadia-HAL/src
)

target_link_libraries(acadia_hal_bridge PRIVATE
    acadia_protos
    spdlog::spdlog
)
