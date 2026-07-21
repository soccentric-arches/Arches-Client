add_library(acadia_hal_service STATIC
    HalServiceImpl.cc
)

target_include_directories(acadia_hal_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../Acadia-HAL/src
)

target_link_libraries(acadia_hal_service PRIVATE
    acadia_registry
    acadia_hal_bridge
    acadia_protos
    ${ACADIA_GRPC_LIBRARIES}
    protobuf::libprotobuf
    spdlog::spdlog
)
