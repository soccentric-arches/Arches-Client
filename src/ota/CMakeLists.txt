add_library(arches_ota STATIC
    OtaClient.cc
)

target_include_directories(arches_ota PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# gRPC runtime + proto message types come transitively from hal_protos_shared.
target_link_libraries(arches_ota PRIVATE
    hal_protos_shared
    spdlog::spdlog
)