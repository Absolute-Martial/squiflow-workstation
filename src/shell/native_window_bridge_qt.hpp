#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include <QObject>
#include <QQuickWindow>

#include <memory>

namespace squiflow::shell {

class NativeWindowBridgeQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool backdropActive READ backdropActive NOTIFY backdropChanged)

  public:
    explicit NativeWindowBridgeQt(QObject* parent = nullptr);
    ~NativeWindowBridgeQt() override;

    bool available() const noexcept;
    bool backdropActive() const noexcept { return backdrop_active_; }

    Q_INVOKABLE bool setup(QQuickWindow* window, bool dark, bool high_contrast);
    Q_INVOKABLE void setAppearance(bool dark, bool high_contrast);

  signals:
    void backdropChanged();

  private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_{};
    bool backdrop_active_{false};
};

}  // namespace squiflow::shell

#endif
