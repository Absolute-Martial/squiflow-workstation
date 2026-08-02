# The protocol repository, consumed as a submodule. Both the workstation and
# the server pin the same commit, so they cannot disagree about what a right is.

add_library(squiflow_protocol STATIC
    src/module_graph.cpp
    src/operation_table.cpp
    src/workflow_table.cpp)

add_library(squiflow::protocol ALIAS squiflow_protocol)

target_include_directories(squiflow_protocol PUBLIC include)

target_link_libraries(squiflow_protocol
    PRIVATE squiflow::warnings squiflow::hardening squiflow::sanitizers)

if(SQUIFLOW_BUILD_TESTS)
    add_executable(squiflow_protocol_test tests/protocol_test.cpp)
    target_link_libraries(squiflow_protocol_test PRIVATE squiflow_protocol)
    add_test(NAME protocol COMMAND squiflow_protocol_test)
endif()
