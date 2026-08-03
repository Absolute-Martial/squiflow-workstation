#pragma once
#include "shell/primitive_mapping.hpp"
#if defined(SQUIFLOW_WITH_QT)
#include <QString>
#include <QStringView>
namespace squiflow::shell {inline QString format_minor_units_qt(std::int64_t amount,QStringView currency,std::uint8_t scale=2){return QString::fromStdString(format_minor_units(amount,currency.toString().toStdString(),scale));}}
#endif
