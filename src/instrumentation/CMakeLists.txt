# Optional Arches-Instrumentation reporting.
#
# find_package is QUIET and the result is optional on purpose. The client must
# build on a host and into an image that has no instrumentation package --
# that is a supported configuration (ARCHES_INSTRUMENTATION = "0"), not a
# degraded one. When the package is absent the library still exists and every
# method is a no-op, so no call site needs a conditional.
add_library(arches_instrumentation STATIC
    Instrumentation.cc
)

target_include_directories(arches_instrumentation PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(arches_instrumentation PRIVATE
    spdlog::spdlog
)

find_package(instrumentation QUIET)
if(instrumentation_FOUND)
    message(STATUS "Arches-Instrumentation found: alarm and metric reporting enabled")
    # Generates the istate stubs (and everything istate.proto imports) into
    # this target. See Arches-Instrumentation/cmake/instrumentationConfig.cmake.
    instrumentation_generate_client(arches_instrumentation PROTOS istate.proto)
    target_link_libraries(arches_instrumentation PRIVATE instrumentation::client_deps)
    target_compile_definitions(arches_instrumentation PRIVATE ARCHES_WITH_INSTRUMENTATION)
else()
    message(STATUS "Arches-Instrumentation not found: alarm and metric reporting compiled out")
endif()
