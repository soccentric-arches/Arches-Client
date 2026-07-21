add_library(acadia_diag STATIC
    DiagSocket.cc
)

target_include_directories(acadia_diag PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(acadia_diag PRIVATE
    spdlog::spdlog
)
