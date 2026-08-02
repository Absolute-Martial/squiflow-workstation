# The module layer.
#
# The framework itself is a library at this level, deliberately in files rather
# than a subdirectory: cmake/ModuleGraph.cmake treats every directory under
# src/modules as a module and fails the build for one that is not registered.
# That check is worth more than the tidiness of a folder.

add_library(squiflow_module_framework STATIC
    registry.cpp)

add_library(squiflow::modules::framework ALIAS squiflow_module_framework)

target_include_directories(squiflow_module_framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(squiflow_module_framework
    PUBLIC  squiflow::protocol squiflow::engine
    PRIVATE squiflow::warnings squiflow::hardening squiflow::sanitizers)

set_target_properties(squiflow_module_framework PROPERTIES FOLDER "modules")

# Each module adds itself here, and declares what it is with
# squiflow_add_module(). A module that exists on disk and is missing from this
# list fails squiflow_check_module_graph() rather than being quietly absent.
#
# Core, in dependency order:
add_subdirectory(administration)
add_subdirectory(parties)
add_subdirectory(catalog)
add_subdirectory(pricing)
add_subdirectory(orders)
add_subdirectory(receivables)
#
# Extra:
add_subdirectory(jobs)
add_subdirectory(quotations)
add_subdirectory(agreements)
add_subdirectory(sourcing)
add_subdirectory(companion)
add_subdirectory(files)

if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/modules/module_framework_test.cpp")
    add_executable(module_framework_test
        "${SQUIFLOW_ROOT}/tests/modules/module_framework_test.cpp")
    target_link_libraries(module_framework_test
        PRIVATE squiflow::modules::framework squiflow::testing
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME modules.framework COMMAND module_framework_test)
endif()
