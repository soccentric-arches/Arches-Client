add_library(acadia_stream STATIC
    StreamClient.cc
)

target_include_directories(acadia_stream PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(acadia_stream PRIVATE
    acadia_protos
    ${ACADIA_GRPC_LIBRARIES}
    spdlog::spdlog
)
