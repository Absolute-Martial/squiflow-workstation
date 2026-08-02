squiflow_add_module(companion
    TIER extra
    SOURCES module.cpp domain/task.cpp data/tables.cpp data/repository.cpp service/companion_service.cpp
    TEST_SOURCES tests/companion_test.cpp)
target_include_directories(squiflow_module_companion PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
if(TARGET squiflow_module_companion_test)
    target_include_directories(squiflow_module_companion_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()
target_link_libraries(squiflow_module_companion PUBLIC squiflow::modules::framework)
