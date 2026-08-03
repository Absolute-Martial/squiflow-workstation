add_library(squiflow_shell STATIC completion_gate.cpp surface_lifecycle.cpp screen_registry.cpp paged_list_cache.cpp)
add_library(squiflow::shell ALIAS squiflow_shell)
target_include_directories(squiflow_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(squiflow_shell PRIVATE squiflow::app squiflow::warnings squiflow::hardening squiflow::sanitizers)
