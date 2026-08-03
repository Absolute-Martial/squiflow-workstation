#pragma once
#if defined(SQUIFLOW_WITH_QT)
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <utility>
namespace squiflow::shell {class GuiDispatcher final{public:explicit GuiDispatcher(QObject& target)noexcept:target_(&target){}template<class F>bool post(F&& f){if(!target_)return false;return QMetaObject::invokeMethod(target_,std::forward<F>(f),Qt::QueuedConnection);}private:QPointer<QObject> target_;};}
#endif
