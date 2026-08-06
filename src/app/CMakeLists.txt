add_library(squiflow_app STATIC
    startup.cpp
    startup_sequence.cpp
    real_startup_runtime.cpp
    activation_controller.cpp
    qt_message_mapping.cpp
    composition_root.cpp)
add_library(squiflow::app ALIAS squiflow_app)
target_include_directories(squiflow_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(squiflow_app
    PUBLIC squiflow::platform squiflow::modules::framework
    PRIVATE squiflow::workflows
    PRIVATE squiflow_module_administration squiflow_module_parties
            squiflow_module_catalog squiflow_module_pricing
            squiflow_module_quotations squiflow_module_orders
            squiflow_module_jobs squiflow_module_receivables
            squiflow_module_agreements squiflow_module_sourcing
            squiflow_module_companion squiflow_module_files
            squiflow::warnings squiflow::hardening squiflow::sanitizers)

add_executable(squiflow_workstation main.cpp)
target_link_libraries(squiflow_workstation PRIVATE squiflow::app)

if(SQUIFLOW_WITH_QT)
    # qt_add_qml_module must be called in the same directory where the target
    # is created: it wires the qmlcachegen/metatype/autogen machinery to the
    # target's own directory scope, and relocating it into a sibling directory
    # makes CMake unable to resolve Qt6::* at generate time. Qt6::* imported
    # targets are provided by the top-level find_package(Qt6) in CMakeLists.txt.
    target_compile_definitions(squiflow_workstation PRIVATE SQUIFLOW_WITH_QT=1)
    set_target_properties(squiflow_workstation PROPERTIES AUTOMOC ON)
    target_link_libraries(squiflow_workstation
        PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    qt_add_qml_module(squiflow_workstation URI SquiFlow VERSION 1.0
        QML_FILES
            ../ui/Main.qml
            ../ui/Theme.qml
            ../ui/common/DataList.qml
            ../ui/common/Form.qml
            ../ui/navigation/NavigationRail.qml
            ../ui/navigation/NavigationDrawer.qml
            ../ui/navigation/NavigationHost.qml
            ../ui/navigation/NoAccessibleModules.qml
            ../ui/screens/ModuleListScreen.qml
        RESOURCES
            ../ui/documents/quotation.html
            ../ui/documents/invoice.html
            ../ui/documents/statement.html
        RESOURCE_PREFIX /qt/qml)
    qt_add_resources(squiflow_workstation squiflow_ui_config
        PREFIX "/" FILES ../ui/qtquickcontrols2.conf)
endif()
