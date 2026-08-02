add_library(squiflow_workflows STATIC
    quote_to_order.cpp)
add_library(squiflow::workflows ALIAS squiflow_workflows)
target_include_directories(squiflow_workflows PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(squiflow_workflows
    PUBLIC squiflow::modules::framework
           squiflow_module_quotations
           squiflow_module_orders
           squiflow_module_pricing
    PRIVATE squiflow::warnings squiflow::hardening squiflow::sanitizers)
if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/workflows/workflow_framework_test.cpp")
    add_executable(workflow_framework_test "${SQUIFLOW_ROOT}/tests/workflows/workflow_framework_test.cpp")
    target_link_libraries(workflow_framework_test PRIVATE squiflow::workflows squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME workflows.framework COMMAND workflow_framework_test)
endif()
if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/workflows/quote_to_order_test.cpp")
    add_executable(quote_to_order_test "${SQUIFLOW_ROOT}/tests/workflows/quote_to_order_test.cpp")
    target_link_libraries(quote_to_order_test PRIVATE squiflow::workflows squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME workflows.quote_to_order COMMAND quote_to_order_test)
endif()
