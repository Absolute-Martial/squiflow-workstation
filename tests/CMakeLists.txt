# Shared dependency-free test harness and the engine suites. Protocol, module,
# and workflow tests are registered beside the libraries they exercise.

add_library(squiflow_testing INTERFACE)
add_library(squiflow::testing ALIAS squiflow_testing)
target_include_directories(squiflow_testing INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

function(squiflow_add_engine_test name source)
    add_executable(${name} "${source}")
    target_link_libraries(${name}
        PRIVATE squiflow::engine squiflow::testing
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    set_target_properties(${name} PROPERTIES FOLDER "tests/engine")
    add_test(NAME "engine.${name}" COMMAND ${name})
endfunction()

squiflow_add_engine_test(engine_domain_test engine/engine_domain_test.cpp)
squiflow_add_engine_test(engine_storage_test engine/engine_storage_test.cpp)
squiflow_add_engine_test(engine_migration_test engine/engine_migration_test.cpp)
squiflow_add_engine_test(engine_writer_test engine/engine_writer_test.cpp)
squiflow_add_engine_test(engine_outbox_test engine/engine_outbox_test.cpp)
squiflow_add_engine_test(engine_sync_test engine/engine_sync_test.cpp)
squiflow_add_engine_test(engine_payload_test engine/engine_payload_test.cpp)

function(squiflow_add_platform_test name source)
    add_executable(${name} "${source}")
    target_link_libraries(${name}
        PRIVATE squiflow::platform squiflow::testing
                squiflow::warnings squiflow::hardening squiflow::sanitizers)
    set_target_properties(${name} PROPERTIES FOLDER "tests/platform")
    add_test(NAME "platform.${name}" COMMAND ${name})
endfunction()

squiflow_add_platform_test(platform_paths_test platform/paths_test.cpp)
squiflow_add_platform_test(platform_logging_test platform/logging_test.cpp)

add_executable(app_contracts_test app/contracts_test.cpp)
target_link_libraries(app_contracts_test
    PRIVATE squiflow::app squiflow::testing
            squiflow::warnings squiflow::hardening squiflow::sanitizers)
set_target_properties(app_contracts_test PROPERTIES FOLDER "tests/app")
add_test(NAME app.contracts COMMAND app_contracts_test)
add_executable(shell_presentation_seam_test shell/presentation_seam_test.cpp)
target_link_libraries(shell_presentation_seam_test PRIVATE squiflow::shell squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
add_test(NAME shell.presentation_seam COMMAND shell_presentation_seam_test)
add_executable(shell_surface_lifecycle_test shell/surface_lifecycle_test.cpp)
target_link_libraries(shell_surface_lifecycle_test PRIVATE squiflow::shell squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
add_test(NAME shell.surface_lifecycle COMMAND shell_surface_lifecycle_test)
add_executable(shell_screen_registry_test shell/screen_registry_test.cpp)
target_link_libraries(shell_screen_registry_test PRIVATE squiflow::shell squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
add_test(NAME shell.screen_registry COMMAND shell_screen_registry_test)
add_executable(shell_paged_list_cache_test shell/paged_list_cache_test.cpp)
target_link_libraries(shell_paged_list_cache_test PRIVATE squiflow::shell squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
add_test(NAME shell.paged_list_cache COMMAND shell_paged_list_cache_test)
add_executable(app_domain_event_bus_test app/domain_event_bus_test.cpp)
target_link_libraries(app_domain_event_bus_test PRIVATE squiflow::app squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
add_test(NAME app.domain_event_bus COMMAND app_domain_event_bus_test)
