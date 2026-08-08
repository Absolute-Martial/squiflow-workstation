#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "shell/notification_queue.hpp"
#include "shell/shell_state.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace squiflow::shell {

class ShellStateQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString tenantName READ tenantName NOTIFY identityChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY identityChanged)
    Q_PROPERTY(int connectivityState READ connectivityState NOTIFY connectivityChanged)
    Q_PROPERTY(int themeChoice READ themeChoice WRITE setThemeChoice NOTIFY themeChanged)
    Q_PROPERTY(bool highContrast READ highContrast WRITE setHighContrast NOTIFY accessibilityChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY accessibilityChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool unsavedDecisionPending READ unsavedDecisionPending NOTIFY unsavedDecisionChanged)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)

  public:
    explicit ShellStateQt(QObject* parent = nullptr) : QObject(parent) {}

    QString tenantName() const;
    QString userName() const;
    int connectivityState() const noexcept;
    int themeChoice() const noexcept;
    bool highContrast() const noexcept { return state_.high_contrast(); }
    bool reducedMotion() const noexcept { return state_.reduced_motion(); }
    bool dirty() const noexcept { return state_.dirty(); }
    bool unsavedDecisionPending() const noexcept {
        return state_.awaiting_unsaved_decision();
    }
    QVariantList notifications() const;

    Q_INVOKABLE bool requestRoute(const QString& stable_id);
    Q_INVOKABLE void resolveUnsaved(bool discard);
    Q_INVOKABLE void setDirty(bool dirty);
    Q_INVOKABLE void setThemeChoice(int choice);
    Q_INVOKABLE void setHighContrast(bool enabled);
    Q_INVOKABLE void setReducedMotion(bool enabled);
    Q_INVOKABLE bool showNotification(const QString& deduplication_key,
                                      const QString& message_key,
                                      const QString& detail, int severity);
    Q_INVOKABLE bool dismissNotification(const QString& id);

  signals:
    void identityChanged();
    void connectivityChanged();
    void themeChanged();
    void accessibilityChanged();
    void dirtyChanged();
    void unsavedDecisionChanged();
    void notificationsChanged();
    void routeApproved(QString stableId);

  private:
    ShellState state_{};
    NotificationQueue notifications_{};
};

}  // namespace squiflow::shell

#endif
