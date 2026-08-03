#include "shell/qml_surface_qt.hpp"
#if defined(SQUIFLOW_WITH_QT)
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>
namespace squiflow::shell {
QmlSurfaceQt::QmlSurfaceQt(SurfaceLifecycle::ShutdownRequest request,QObject*p):QObject(p),lifecycle_(std::move(request)){}
app::StepResult QmlSurfaceQt::startShell(){if(engine_)return{app::StepDisposition::Failed,"QML engine already exists"};engine_=std::make_unique<QQmlApplicationEngine>();engine_->rootContext()->setContextProperty("applicationSurface",this);connect(engine_.get(),&QQmlApplicationEngine::objectCreationFailed,this,[this]{creation_failed_=true;});return lifecycle_.start_shell(true);}
app::StepResult QmlSurfaceQt::startWindow(){if(!engine_)return lifecycle_.start_window(false,"QML engine missing");creation_failed_=false;engine_->loadFromModule("SquiFlow","Main");if(creation_failed_||engine_->rootObjects().isEmpty())return lifecycle_.start_window(false,"QML root creation failed");root_=qobject_cast<QWindow*>(engine_->rootObjects().constFirst());if(!root_)return lifecycle_.start_window(false,"QML root is not a window");root_->show();return lifecycle_.start_window(true);}
void QmlSurfaceQt::requestShutdown(){lifecycle_.request_shutdown();}
void QmlSurfaceQt::stopWindow()noexcept{if(root_){root_->hide();delete root_.data();root_=nullptr;}lifecycle_.stop_window();}
void QmlSurfaceQt::stopShell()noexcept{engine_.reset();lifecycle_.stop_shell();}
}
#endif
