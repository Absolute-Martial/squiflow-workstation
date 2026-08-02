squiflow_add_module(receivables
    TIER core
    SOURCES
        module.cpp
        domain/invoice.cpp
        domain/payment.cpp
        domain/credit_account.cpp
        domain/statement.cpp
        data/tables.cpp
        data/repository.cpp
        service/receivables_service.cpp
    TEST_SOURCES
        tests/receivables_test.cpp)

target_include_directories(squiflow_module_receivables
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(TARGET squiflow_module_receivables_test)
    target_include_directories(squiflow_module_receivables_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

target_link_libraries(squiflow_module_receivables
    PUBLIC squiflow::modules::framework
           squiflow_module_parties
           squiflow_module_pricing)
