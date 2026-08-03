# A verification lane for the real CMake files.
#
# The top-level CMakeLists.txt cannot be configured until every phase exists,
# because it adds src/app, src/ui, src/shell and the rest. That would mean the
# cmake/ helpers stay unexecuted until the last day of the project, which is
# exactly when a mistake in them is most expensive.
#
# So this project includes the same helpers, from the same files, and adds the
# real targets that exist today. It is deleted when the top-level file can be
# configured on its own. It must never grow a copy of anything - if a rule is
# defined here rather than included from cmake/, this lane is verifying itself
# instead of the build.

cmake_minimum_required(VERSION 3.26)

get_filename_component(SQUIFLOW_REAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(SQUIFLOW_ROOT "${SQUIFLOW_REAL_ROOT}" CACHE INTERNAL "SquiFlow source root")

list(APPEND CMAKE_MODULE_PATH "${SQUIFLOW_ROOT}/cmake")

include(Version)

project(SquiFlowVerify
    VERSION ${SQUIFLOW_VERSION}
    DESCRIPTION "Verification lane for the SquiFlow build files"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# The same warning, hardening, sanitizer, module and graph rules the real build
# uses. Included, never copied.
include(Warnings)
include(Hardening)
include(Sanitizers)
include(SquiflowModule)
include(ModuleGraph)

option(SQUIFLOW_BUILD_TESTS "Build the test targets" ON)

if(SQUIFLOW_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        URL "${SQUIFLOW_ROOT}/external/dist/googletest-1.17.0.tar.gz"
        URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c)
    FetchContent_MakeAvailable(googletest)
endif()

# The test support header, as a target, so module tests can ask for
# squiflow::testing exactly as SquiflowModule.cmake expects.
add_library(squiflow_testing INTERFACE)
add_library(squiflow::testing ALIAS squiflow_testing)
target_include_directories(squiflow_testing INTERFACE ${SQUIFLOW_ROOT}/tests)

add_subdirectory(${SQUIFLOW_ROOT}/external/protocol protocol)
add_subdirectory(${SQUIFLOW_ROOT}/src/platform platform)

# This lane verifies protocol, engine mechanisms, modules and the real CMake
# helpers on machines that do not carry the production SQLite toolchain. Keep
# this as a default rather than FORCE: a caller with SQLite >= 3.51.3 may still
# pass -DSQUIFLOW_WITH_SQLITE=ON and verify the persistent backing too. The
# engine emits a visible warning when this is off, so this lane cannot be
# mistaken for a shippable build.
set(SQUIFLOW_WITH_SQLITE OFF CACHE BOOL
    "Build SQLite in the non-shippable CMake verification lane")
add_subdirectory(${SQUIFLOW_ROOT}/src/engine engine)

if(EXISTS "${SQUIFLOW_ROOT}/src/modules/CMakeLists.txt")
    add_subdirectory(${SQUIFLOW_ROOT}/src/modules modules)

    # The graph check refuses a build with no modules, which is correct for the
    # real build and wrong here while the framework exists and the modules do
    # not yet. Once the first module is declared this runs on every configure.
    get_property(_declared GLOBAL PROPERTY SQUIFLOW_MODULE_LIST)
    if(_declared)
        squiflow_check_module_graph()
    endif()
endif()

if(EXISTS "${SQUIFLOW_ROOT}/src/workflows/CMakeLists.txt")
    add_subdirectory(${SQUIFLOW_ROOT}/src/workflows workflows)
endif()

if(EXISTS "${SQUIFLOW_ROOT}/src/app/CMakeLists.txt")
    add_subdirectory(${SQUIFLOW_ROOT}/src/app app)
endif()

if(EXISTS "${SQUIFLOW_ROOT}/src/shell/CMakeLists.txt")
    add_subdirectory(${SQUIFLOW_ROOT}/src/shell shell)
endif()

# Bridge contracts are part of the portable verification lane.
if(SQUIFLOW_BUILD_TESTS)
    function(squiflow_add_bridge_test target test_name source)
        add_executable(${target} "${SQUIFLOW_ROOT}/${source}")
        target_link_libraries(${target} PRIVATE squiflow::testing
            squiflow::warnings squiflow::hardening squiflow::sanitizers ${ARGN})
        add_test(NAME ${test_name} COMMAND ${target})
    endfunction()
    squiflow_add_bridge_test(app_contracts_test app.contracts tests/app/contracts_test.cpp squiflow::app)
    squiflow_add_bridge_test(shell_presentation_seam_test shell.presentation_seam tests/shell/presentation_seam_test.cpp squiflow::shell)
    squiflow_add_bridge_test(shell_surface_lifecycle_test shell.surface_lifecycle tests/shell/surface_lifecycle_test.cpp squiflow::shell)
    squiflow_add_bridge_test(shell_screen_registry_test shell.screen_registry tests/shell/screen_registry_test.cpp squiflow::shell)
    squiflow_add_bridge_test(shell_paged_list_cache_test shell.paged_list_cache tests/shell/paged_list_cache_test.cpp squiflow::shell)
    squiflow_add_bridge_test(app_domain_event_bus_test app.domain_event_bus tests/app/domain_event_bus_test.cpp squiflow::app)
endif()

# The engine tests, which the real tests/CMakeLists.txt will own once it exists.
if(SQUIFLOW_BUILD_TESTS)
    add_executable(platform_single_instance_test
        ${SQUIFLOW_ROOT}/tests/platform/single_instance_test.cpp)
    target_link_libraries(platform_single_instance_test
        PRIVATE squiflow::platform squiflow::testing
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME platform.single_instance COMMAND platform_single_instance_test)

    add_executable(platform_secrets_test
        ${SQUIFLOW_ROOT}/tests/platform/secrets_test.cpp)
    target_link_libraries(platform_secrets_test
        PRIVATE squiflow::platform squiflow::testing
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME platform.secrets COMMAND platform_secrets_test)

    add_executable(platform_secrets_gtest
        ${SQUIFLOW_ROOT}/tests/platform/secrets_gtest.cpp)
    target_link_libraries(platform_secrets_gtest
        PRIVATE squiflow::platform GTest::gtest_main
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME platform.secrets.google_test COMMAND platform_secrets_gtest)

    add_executable(platform_network_state_test
        ${SQUIFLOW_ROOT}/tests/platform/network_state_test.cpp)
    target_link_libraries(platform_network_state_test
        PRIVATE squiflow::platform squiflow::testing
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME platform.network_state COMMAND platform_network_state_test)

    add_executable(platform_background_services_test
        ${SQUIFLOW_ROOT}/tests/platform/background_services_test.cpp)
    target_link_libraries(platform_background_services_test
        PRIVATE squiflow::platform squiflow::testing
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME platform.background_services COMMAND platform_background_services_test)


    add_executable(app_startup_test
        ${SQUIFLOW_ROOT}/tests/app/startup_test.cpp)
    target_link_libraries(app_startup_test
        PRIVATE squiflow::app squiflow::testing
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME app.startup COMMAND app_startup_test)

    foreach(t domain storage migration writer outbox sync)
        set(_src "${SQUIFLOW_ROOT}/tests/engine/engine_${t}_test.cpp")
        if(EXISTS "${_src}")
            add_executable(engine_${t}_test "${_src}")
            target_link_libraries(engine_${t}_test
                PRIVATE squiflow::engine squiflow::testing
                        squiflow::warnings squiflow::hardening squiflow::sanitizers)
            add_test(NAME engine.${t} COMMAND engine_${t}_test)
        endif()
    endforeach()
endif()