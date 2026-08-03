#pragma once
#if defined(SQUIFLOW_WITH_QT)
#include "shell/surface_lifecycle.hpp"
#include <QObject>
#include <QPointer>
#include <memory>
class QQmlApplicationEngine;class QWindow;
namespace squiflow::shell {
class QmlSurfaceQt final:public QObject{Q_OBJECT public:explicit QmlSurfaceQt(SurfaceLifecycle::ShutdownRequest request,QObject* parent=nullptr);app::StepResult startShell();app::StepResult startWindow();Q_INVOKABLE void requestShutdown();void stopWindow()noexcept;void stopShell()noexcept;private:SurfaceLifecycle lifecycle_;std::unique_ptr<QQmlApplicationEngine> engine_;QPointer<QWindow> root_;bool creation_failed_{false};};
}
#endif
