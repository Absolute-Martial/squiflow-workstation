add_library(squiflow_app STATIC
    startup.cpp
    startup_sequence.cpp
    activation_controller.cpp
    qt_message_mapping.cpp
    composition_root.cpp)
add_library(squiflow::app ALIAS squiflow_app)
target_include_directories(squiflow_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(squiflow_app
    PUBLIC squiflow::platform squiflow::modules::framework
    PRIVATE squiflow_module_administration squiflow_module_parties
            squiflow_module_catalog squiflow_module_pricing
            squiflow_module_quotations squiflow_module_orders
            squiflow_module_jobs squiflow_module_receivables
            squiflow_module_agreements squiflow_module_sourcing
            squiflow_module_companion squiflow_module_files
            squiflow::warnings squiflow::hardening squiflow::sanitizers)

add_executable(squiflow_workstation main.cpp)
target_link_libraries(squiflow_workstation PRIVATE squiflow::app)
