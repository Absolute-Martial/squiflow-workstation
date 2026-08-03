#include "shell/paged_list_model_qt.hpp"
#include "shell/primitive_mapping_qt.hpp"
#include <QCoreApplication>
#include <QString>
#include <thread>
int main(int argc,char**argv){QCoreApplication app(argc,argv);using namespace squiflow::shell;if(format_minor_units_qt(-5,u"USD")!=QStringLiteral("-0.05 USD"))return 1;PagedListModelQt model;auto generation=model.beginQuery();std::thread worker([&]{model.applyPage(generation,{{"1","One","Detail"}});});worker.join();QCoreApplication::processEvents();if(model.rowCount()!=1||model.data(model.index(0),PagedListModelQt::StableIdRole).toString()!=QStringLiteral("1"))return 2;return 0;}
