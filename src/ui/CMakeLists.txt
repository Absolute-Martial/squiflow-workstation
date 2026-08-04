if(SQUIFLOW_WITH_QT)
    find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
    qt_standard_project_setup(REQUIRES 6.11.1)
    target_compile_definitions(squiflow_workstation PRIVATE SQUIFLOW_WITH_QT=1)
    target_compile_definitions(squiflow_shell PRIVATE SQUIFLOW_WITH_QT=1)
    set_target_properties(squiflow_shell PROPERTIES AUTOMOC ON)
    target_sources(squiflow_shell PRIVATE
        ../shell/paged_list_model_qt.cpp
        ../shell/list_screen_bridge_qt.cpp
        ../shell/navigation_model_qt.cpp
        ../shell/navigation_bridge_qt.cpp
        ../shell/qml_surface_qt.cpp)
    target_link_libraries(squiflow_shell PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
    target_link_libraries(squiflow_workstation PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 squiflow::shell)
    qt_add_qml_module(squiflow_workstation URI SquiFlow VERSION 1.0
        QML_FILES
            Main.qml
            Theme.qml
            common/DataList.qml
            navigation/NavigationRail.qml
            navigation/NavigationDrawer.qml
            navigation/NavigationHost.qml
            navigation/NoAccessibleModules.qml
            screens/ModuleListScreen.qml
        RESOURCE_PREFIX /qt/qml)
    qt_add_resources(squiflow_workstation squiflow_ui_config PREFIX "/" FILES qtquickcontrols2.conf)

    if(SQUIFLOW_BUILD_TESTS)
        add_executable(qt_bridge_test ../../tests/shell/qt_bridge_test.cpp)
        target_link_libraries(qt_bridge_test PRIVATE squiflow::shell squiflow::testing Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick squiflow::warnings squiflow::hardening squiflow::sanitizers)
        add_test(NAME shell.qt_bridge COMMAND qt_bridge_test)
    endif()
endif()
