add_library(acadia_ota STATIC
    OtaClient.cc
)

target_include_directories(acadia_ota PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(acadia_ota PRIVATE
    acadia_protos
    ${ACADIA_GRPC_LIBRARIES}
    spdlog::spdlog
)
