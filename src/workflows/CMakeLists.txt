add_library(squiflow_workflows STATIC
    quote_to_order.cpp
    order_to_jobs.cpp
    issue_invoice.cpp
    cancel_and_reissue.cpp
    take_payment.cpp)
add_library(squiflow::workflows ALIAS squiflow_workflows)
target_include_directories(squiflow_workflows PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(squiflow_workflows
    PUBLIC squiflow::modules::framework
           squiflow_module_quotations
           squiflow_module_orders
           squiflow_module_jobs
           squiflow_module_pricing
           squiflow_module_receivables
    PRIVATE squiflow::warnings squiflow::hardening squiflow::sanitizers)
if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/workflows/workflow_framework_test.cpp")
    add_executable(workflow_framework_test "${SQUIFLOW_ROOT}/tests/workflows/workflow_framework_test.cpp")
    target_link_libraries(workflow_framework_test PRIVATE squiflow::workflows squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME workflows.framework COMMAND workflow_framework_test)
endif()
if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/workflows/issue_invoice_test.cpp")
    add_executable(issue_invoice_test "${SQUIFLOW_ROOT}/tests/workflows/issue_invoice_test.cpp")
    target_link_libraries(issue_invoice_test PRIVATE squiflow::workflows squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME workflows.issue_invoice COMMAND issue_invoice_test)
endif()
if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/workflows/cancel_and_reissue_test.cpp")
    add_executable(cancel_and_reissue_test "${SQUIFLOW_ROOT}/tests/workflows/cancel_and_reissue_test.cpp")
    target_link_libraries(cancel_and_reissue_test PRIVATE squiflow::workflows squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME workflows.cancel_and_reissue COMMAND cancel_and_reissue_test)
endif()
if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/workflows/take_payment_test.cpp")
    add_executable(take_payment_test "${SQUIFLOW_ROOT}/tests/workflows/take_payment_test.cpp")
    target_link_libraries(take_payment_test PRIVATE squiflow::workflows squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME workflows.take_payment COMMAND take_payment_test)
endif()
if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/workflows/quote_to_order_test.cpp")
    add_executable(quote_to_order_test "${SQUIFLOW_ROOT}/tests/workflows/quote_to_order_test.cpp")
    target_link_libraries(quote_to_order_test PRIVATE squiflow::workflows squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME workflows.quote_to_order COMMAND quote_to_order_test)
endif()
if(SQUIFLOW_BUILD_TESTS AND EXISTS "${SQUIFLOW_ROOT}/tests/workflows/order_to_jobs_test.cpp")
    add_executable(order_to_jobs_test "${SQUIFLOW_ROOT}/tests/workflows/order_to_jobs_test.cpp")
    target_link_libraries(order_to_jobs_test PRIVATE squiflow::workflows squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
    add_test(NAME workflows.order_to_jobs COMMAND order_to_jobs_test)
endif()
