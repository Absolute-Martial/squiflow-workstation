#include "shell/shell_state_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include <QVariantMap>

namespace squiflow::shell {
namespace {

QString text(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

}  // namespace

QString ShellStateQt::tenantName() const { return text(state_.tenant_name()); }
QString ShellStateQt::userName() const { return text(state_.user_name()); }
int ShellStateQt::connectivityState() const noexcept {
    return static_cast<int>(state_.connectivity());
}
int ShellStateQt::themeChoice() const noexcept {
    return static_cast<int>(state_.theme());
}

QVariantList ShellStateQt::notifications() const {
    QVariantList result;
    for (const auto& item : notifications_.items()) {
        result.push_back(QVariantMap{
            {QStringLiteral("id"), text(item.id)},
            {QStringLiteral("messageKey"), text(item.message_key)},
            {QStringLiteral("detail"), text(item.detail)},
            {QStringLiteral("severity"), static_cast<int>(item.severity)},
            {QStringLiteral("occurrences"), static_cast<qulonglong>(item.occurrences)}});
    }
    return result;
}

bool ShellStateQt::requestRoute(const QString& stable_id) {
    const bool approved = state_.request_route(stable_id.toStdString());
    emit unsavedDecisionChanged();
    if (approved) {
        emit routeApproved(stable_id);
    }
    return approved;
}

void ShellStateQt::resolveUnsaved(bool discard) {
    const bool was_dirty = state_.dirty();
    const auto route = state_.resolve_unsaved(discard);
    emit unsavedDecisionChanged();
    if (was_dirty != state_.dirty()) {
        emit dirtyChanged();
    }
    if (route) {
        emit routeApproved(text(*route));
    }
}

void ShellStateQt::setDirty(bool dirty_value) {
    if (dirty_value == state_.dirty()) {
        return;
    }
    state_.mark_dirty(dirty_value);
    emit dirtyChanged();
    emit unsavedDecisionChanged();
}

void ShellStateQt::setThemeChoice(int choice) {
    if (choice < static_cast<int>(ThemeChoice::System) ||
        choice > static_cast<int>(ThemeChoice::Dark) ||
        choice == themeChoice()) {
        return;
    }
    state_.set_theme(static_cast<ThemeChoice>(choice));
    emit themeChanged();
}

void ShellStateQt::setHighContrast(bool enabled) {
    if (enabled == state_.high_contrast()) {
        return;
    }
    state_.set_high_contrast(enabled);
    emit accessibilityChanged();
}

void ShellStateQt::setReducedMotion(bool enabled) {
    if (enabled == state_.reduced_motion()) {
        return;
    }
    state_.set_reduced_motion(enabled);
    emit accessibilityChanged();
}

bool ShellStateQt::showNotification(const QString& deduplication_key,
                                    const QString& message_key,
                                    const QString& detail, int severity) {
    if (severity < static_cast<int>(NotificationSeverity::Information) ||
        severity > static_cast<int>(NotificationSeverity::Error)) {
        return false;
    }
    const bool accepted = notifications_.push(
        deduplication_key.toStdString(), message_key.toStdString(),
        detail.toStdString(), static_cast<NotificationSeverity>(severity));
    if (accepted) {
        emit notificationsChanged();
    }
    return accepted;
}

bool ShellStateQt::dismissNotification(const QString& id) {
    const bool dismissed = notifications_.dismiss(id.toStdString());
    if (dismissed) {
        emit notificationsChanged();
    }
    return dismissed;
}

}  // namespace squiflow::shell

#endif
