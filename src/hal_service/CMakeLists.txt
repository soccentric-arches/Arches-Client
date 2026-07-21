add_library(arches_hal_service STATIC
    HalServiceImpl.cc
)

target_include_directories(arches_hal_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Arches-HAL/src
)

# gRPC runtime + protobuf come transitively from hal_protos_shared.
target_link_libraries(arches_hal_service PRIVATE
    arches_registry
    arches_hal_bridge
    hal_protos_shared
    spdlog::spdlog
)