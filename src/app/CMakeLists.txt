add_library(squiflow_app STATIC
    startup.cpp
    startup_sequence.cpp
    real_startup_runtime.cpp
    activation_controller.cpp
    qt_message_mapping.cpp
    composition_root.cpp
    dashboard/dashboard_service.cpp
    primary/primary_page_service.cpp
    primary/local_primary_query.cpp
    primary/record_page_service.cpp
    primary/local_record_query.cpp
    primary/command_gateway.cpp
    session_authorization.cpp
    workspace_runtime.cpp
    credential_authentication.cpp
    credential_command.cpp)
add_library(squiflow::app ALIAS squiflow_app)
target_include_directories(squiflow_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(squiflow_app
    PUBLIC squiflow::platform squiflow::modules::framework
    PRIVATE squiflow::workflows
    PRIVATE squiflow_module_administration squiflow_module_parties
            squiflow_module_catalog squiflow_module_pricing
            squiflow_module_quotations squiflow_module_orders
            squiflow_module_jobs squiflow_module_receivables
            squiflow_module_agreements squiflow_module_sourcing
            squiflow_module_companion squiflow_module_files
            squiflow::warnings squiflow::hardening squiflow::sanitizers)

if(SQUIFLOW_USE_PCH AND SQUIFLOW_COMMON_PCH_HEADERS)
    target_precompile_headers(squiflow_app PRIVATE ${SQUIFLOW_COMMON_PCH_HEADERS})
endif()

# The squiflow_workstation executable and its QML module are declared in
# src/ui/CMakeLists.txt, in the directory that owns the module files. The Qt
# tooling (qmlcachegen, metatypes, autogen) derives every output path from the
# module directory: declaring the module from a sibling directory would make
# it emit "<target>_../ui/..." paths that the Ninja generator's mkdir cannot
# create.