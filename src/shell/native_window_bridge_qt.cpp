#include "shell/native_window_bridge_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include <QQuickWindow>

#if defined(SQUIFLOW_WITH_QWINDOWKIT)
#include <QWKQuick/quickwindowagent.h>
#endif

namespace squiflow::shell {

class NativeWindowBridgeQt::Implementation final {
  public:
#if defined(SQUIFLOW_WITH_QWINDOWKIT)
    QWK::QuickWindowAgent agent{};
#endif
    bool setup{false};
};

NativeWindowBridgeQt::NativeWindowBridgeQt(QObject* parent)
    : QObject(parent), implementation_(std::make_unique<Implementation>()) {}

NativeWindowBridgeQt::~NativeWindowBridgeQt() = default;

bool NativeWindowBridgeQt::available() const noexcept {
#if defined(SQUIFLOW_WITH_QWINDOWKIT)
    return true;
#else
    return false;
#endif
}

bool NativeWindowBridgeQt::setup(QQuickWindow* window, bool dark,
                                 bool high_contrast) {
#if defined(SQUIFLOW_WITH_QWINDOWKIT)
    if (window == nullptr) {
        return false;
    }
    if (!implementation_->setup) {
        implementation_->setup = implementation_->agent.setup(window);
    }
    if (!implementation_->setup) {
        return false;
    }
    setAppearance(dark, high_contrast);
    return true;
#else
    Q_UNUSED(window)
    Q_UNUSED(dark)
    Q_UNUSED(high_contrast)
    return false;
#endif
}

void NativeWindowBridgeQt::setAppearance(bool dark, bool high_contrast) {
#if defined(SQUIFLOW_WITH_QWINDOWKIT)
    if (!implementation_->setup) {
        return;
    }
    (void)implementation_->agent.setWindowAttribute(QStringLiteral("dark-mode"), dark);
    bool active = false;
#if defined(Q_OS_WIN)
    if (!high_contrast) {
        active = implementation_->agent.setWindowAttribute(QStringLiteral("mica"), true);
        if (!active) {
            active = implementation_->agent.setWindowAttribute(
                QStringLiteral("acrylic-material"), true);
        }
    } else {
        (void)implementation_->agent.setWindowAttribute(QStringLiteral("mica"), false);
        (void)implementation_->agent.setWindowAttribute(
            QStringLiteral("acrylic-material"), false);
    }
#endif
    if (active != backdrop_active_) {
        backdrop_active_ = active;
        emit backdropChanged();
    }
#else
    Q_UNUSED(dark)
    Q_UNUSED(high_contrast)
#endif
}

}  // namespace squiflow::shell

#endif
