squiflow_add_module(jobs
    TIER extra
    SOURCES
        module.cpp
        domain/job.cpp
        data/tables.cpp
        data/repository.cpp
        service/jobs_service.cpp
    TEST_SOURCES
        tests/jobs_test.cpp)

target_include_directories(squiflow_module_jobs
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(TARGET squiflow_module_jobs_test)
    target_include_directories(squiflow_module_jobs_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

target_link_libraries(squiflow_module_jobs
    PUBLIC squiflow::modules::framework)
