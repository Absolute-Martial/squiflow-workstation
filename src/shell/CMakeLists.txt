add_library(squiflow_shell STATIC completion_gate.cpp surface_lifecycle.cpp screen_registry.cpp navigation_manifest.cpp navigation_controller.cpp paged_list_cache.cpp list_bridge.cpp window_state.cpp form_bridge.cpp)
add_library(squiflow::shell ALIAS squiflow_shell)
target_include_directories(squiflow_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(squiflow_shell PUBLIC squiflow::protocol squiflow::engine PRIVATE squiflow::app squiflow::warnings squiflow::hardening squiflow::sanitizers)
