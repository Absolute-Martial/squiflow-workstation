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
        qml/Main.qml
            qml/Theme.qml
            qml/common/DataList.qml
            qml/common/Form.qml
            qml/common/FilePreview.qml
            qml/common/PageScaffold.qml
            qml/common/CommandBar.qml
            qml/common/StatusBanner.qml
            qml/common/MetricCard.qml
            qml/common/SectionCard.qml
            qml/common/EmptyState.qml
            qml/common/ErrorState.qml
            qml/common/LoadingSkeleton.qml
            qml/common/ConfirmDialog.qml
            qml/common/UnsavedChangesDialog.qml
            qml/common/PrimaryListPage.qml
            qml/common/OperationalModulePage.qml
            qml/common/RecordDetailPane.qml
            qml/common/RecordActionDialog.qml
            qml/dashboard/DashboardPage.qml
            qml/parties/PartiesPage.qml
            qml/parties/PartyDetailPage.qml
            qml/parties/PartyFormPage.qml
            qml/parties/ContactEditor.qml
            qml/catalog/CatalogPage.qml
            qml/catalog/ProductDetailPage.qml
            qml/catalog/ProductFormPage.qml
            qml/pricing/PricingPage.qml
            qml/pricing/RateDetailPage.qml
            qml/pricing/RateFormPage.qml
            qml/pricing/PriceResolutionPanel.qml
            qml/orders/OrdersPage.qml
            qml/orders/OrderDetailPage.qml
            qml/orders/OrderEditorPage.qml
            qml/orders/OrderLineEditor.qml
            qml/orders/PriceEvidenceBadge.qml
            qml/counter/CounterSalePage.qml
            qml/counter/CartLine.qml
            qml/counter/PaymentPanel.qml
            qml/receivables/ReceivablesPage.qml
            qml/receivables/InvoiceDetailPage.qml
            qml/receivables/PaymentAllocationPage.qml
            qml/receivables/StatementPanel.qml
            qml/receivables/DocumentDeliveryPanel.qml
            qml/quotations/QuotationsPage.qml
            qml/agreements/AgreementsPage.qml
            qml/jobs/JobsPage.qml
            qml/sourcing/SourcingPage.qml
            qml/companion/CompanionPage.qml
            qml/files/FilesPage.qml
            qml/administration/AdministrationPage.qml
            qml/navigation/NavigationRail.qml
            qml/navigation/NavigationDrawer.qml
            qml/navigation/NavigationHost.qml
            qml/navigation/NoAccessibleModules.qml
            qml/screens/ModuleListScreen.qml)
    set(_squiflow_ui_resources
        qml/documents/quotation.html
        qml/documents/invoice.html
        qml/documents/statement.html)
    qt_add_qml_module(squiflow_workstation URI SquiFlow VERSION 1.0
        QML_FILES ${_squiflow_qml_sources}
        RESOURCES ${_squiflow_ui_resources}
        RESOURCE_PREFIX /qt/qml)
    qt_add_resources(squiflow_workstation squiflow_ui_config
        PREFIX "/" FILES qml/qtquickcontrols2.conf)
endif()
