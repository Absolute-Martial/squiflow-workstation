add_library(squiflow_app STATIC
    startup.cpp
    startup_sequence.cpp
    real_startup_runtime.cpp
    activation_controller.cpp
    qt_message_mapping.cpp
    composition_root.cpp
    dashboard/dashboard_service.cpp
    primary/primary_page_service.cpp
    primary/local_primary_query.cpp
    primary/record_page_service.cpp
    primary/local_record_query.cpp
    primary/command_gateway.cpp
    session_authorization.cpp
    workspace_runtime.cpp
    credential_authentication.cpp
    credential_command.cpp)
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
    # The QML tree lives at qml/ inside this directory. qmlcachegen derives
    # its cache directory from each file's path relative to the target
    # directory; paths walking up through ".." would produce a cache
    # directory whose last component ends in a dot, which Windows refuses to
    # create. Keeping the tree below the target keeps those names clean on
    # every platform.
    set(_squiflow_qml_sources
        ../ui/Main.qml
            ../ui/Theme.qml
            ../ui/common/DataList.qml
            ../ui/common/Form.qml
            ../ui/common/FilePreview.qml
            ../ui/common/PageScaffold.qml
            ../ui/common/CommandBar.qml
            ../ui/common/StatusBanner.qml
            ../ui/common/MetricCard.qml
            ../ui/common/SectionCard.qml
            ../ui/common/EmptyState.qml
            ../ui/common/ErrorState.qml
            ../ui/common/LoadingSkeleton.qml
            ../ui/common/ConfirmDialog.qml
            ../ui/common/UnsavedChangesDialog.qml
            ../ui/common/PrimaryListPage.qml
            ../ui/common/OperationalModulePage.qml
            ../ui/common/RecordDetailPane.qml
            ../ui/common/RecordActionDialog.qml
            ../ui/dashboard/DashboardPage.qml
            ../ui/parties/PartiesPage.qml
            ../ui/parties/PartyDetailPage.qml
            ../ui/parties/PartyFormPage.qml
            ../ui/parties/ContactEditor.qml
            ../ui/catalog/CatalogPage.qml
            ../ui/catalog/ProductDetailPage.qml
            ../ui/catalog/ProductFormPage.qml
            ../ui/pricing/PricingPage.qml
            ../ui/pricing/RateDetailPage.qml
            ../ui/pricing/RateFormPage.qml
            ../ui/pricing/PriceResolutionPanel.qml
            ../ui/orders/OrdersPage.qml
            ../ui/orders/OrderDetailPage.qml
            ../ui/orders/OrderEditorPage.qml
            ../ui/orders/OrderLineEditor.qml
            ../ui/orders/PriceEvidenceBadge.qml
            ../ui/counter/CounterSalePage.qml
            ../ui/counter/CartLine.qml
            ../ui/counter/PaymentPanel.qml
            ../ui/receivables/ReceivablesPage.qml
            ../ui/receivables/InvoiceDetailPage.qml
            ../ui/receivables/PaymentAllocationPage.qml
            ../ui/receivables/StatementPanel.qml
            ../ui/receivables/DocumentDeliveryPanel.qml
            ../ui/quotations/QuotationsPage.qml
            ../ui/agreements/AgreementsPage.qml
            ../ui/jobs/JobsPage.qml
            ../ui/sourcing/SourcingPage.qml
            ../ui/companion/CompanionPage.qml
            ../ui/files/FilesPage.qml
            ../ui/administration/AdministrationPage.qml
            ../ui/navigation/NavigationRail.qml
            ../ui/navigation/NavigationDrawer.qml
            ../ui/navigation/NavigationHost.qml
            ../ui/navigation/NoAccessibleModules.qml
            ../ui/screens/ModuleListScreen.qml)
    set(_squiflow_ui_resources
        ../ui/documents/quotation.html
        ../ui/documents/invoice.html
        ../ui/documents/statement.html)
    qt_add_qml_module(squiflow_workstation URI SquiFlow VERSION 1.0
        QML_FILES ${_squiflow_qml_sources}
        RESOURCES ${_squiflow_ui_resources}
        RESOURCE_PREFIX /qt/qml)
    qt_add_resources(squiflow_workstation squiflow_ui_config
        PREFIX "/" FILES ../ui/qtquickcontrols2.conf)
endif()
