add_library(arches_stream STATIC
    StreamClient.cc
)

target_include_directories(arches_stream PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# gRPC runtime + proto message types come transitively from hal_protos_shared.
target_link_libraries(arches_stream PRIVATE
    hal_protos_shared
    spdlog::spdlog
)