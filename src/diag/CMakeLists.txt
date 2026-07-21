add_library(arches_diag STATIC
    DiagSocket.cc
)

target_include_directories(arches_diag PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(arches_diag PRIVATE
    spdlog::spdlog
)