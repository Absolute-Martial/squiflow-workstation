#include "shell/record_screen_bridge_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include "engine/records/payload.hpp"

#include <QDateTime>
#include <QMetaType>
#include <QUuid>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>

namespace squiflow::shell {
namespace {
constexpr qsizetype kMaximumFields = 48;
constexpr qsizetype kMaximumKeyBytes = 128;
constexpr qsizetype kMaximumTextBytes = 4096;

bool safe_key(const QByteArray& value) {
    if (value.isEmpty() || value.size() > kMaximumKeyBytes) return false;
    return std::all_of(value.begin(), value.end(), [](char raw) {
        const auto c = static_cast<unsigned char>(raw);
        return std::isalnum(c) != 0 || c == static_cast<unsigned char>('_') ||
               c == static_cast<unsigned char>('-') ||
               c == static_cast<unsigned char>('.');
    });
}

bool forbidden_projection_key(const QString& key) {
    const QString lowered = key.toLower();
    return lowered == QStringLiteral("password_hash") ||
           lowered == QStringLiteral("device_id") ||
           lowered == QStringLiteral("volume_id") ||
           lowered == QStringLiteral("path") ||
           lowered == QStringLiteral("stack_trace");
}

QString label_from_key(std::string_view key) {
    QString result = QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
    result.replace(QLatin1Char('.'), QLatin1Char(' '));
    result.replace(QLatin1Char('_'), QLatin1Char(' '));
    return result;
}
}  // namespace

RecordScreenBridgeQt::RecordScreenBridgeQt(
    app::primary::PageKind kind, RecordLoader loader,
    CommandDispatcher dispatcher,
    std::optional<protocol::OperationId> create_operation, QObject* parent)
    : QObject(parent), kind_(kind), loader_(std::move(loader)),
      dispatcher_(std::move(dispatcher)), create_operation_(create_operation) {}

QString RecordScreenBridgeQt::title() const {
    return snapshot_ ? QString::fromStdString(snapshot_->title) : QString{};
}

QString RecordScreenBridgeQt::subtitle() const {
    return snapshot_ ? QString::fromStdString(snapshot_->subtitle) : QString{};
}

QVariantList RecordScreenBridgeQt::fields() const {
    QVariantList result;
    if (!snapshot_) return result;
    result.reserve(static_cast<qsizetype>(snapshot_->fields.size()));
    for (const auto& field : snapshot_->fields) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QString::fromStdString(field.id));
        item.insert(QStringLiteral("label"), label_from_key(field.label_key));
        item.insert(QStringLiteral("value"), QString::fromStdString(field.value_text));
        if (field.exact_minor_units) {
            item.insert(QStringLiteral("exactMinorUnits"),
                        QVariant::fromValue<qlonglong>(*field.exact_minor_units));
        }
        if (field.exact_scaled_units) {
            item.insert(QStringLiteral("exactScaledUnits"),
                        QVariant::fromValue<qlonglong>(*field.exact_scaled_units));
        }
        result.push_back(item);
    }
    return result;
}

QVariantList RecordScreenBridgeQt::lines() const {
    QVariantList result;
    if (!snapshot_) return result;
    result.reserve(static_cast<qsizetype>(snapshot_->lines.size()));
    for (const auto& line : snapshot_->lines) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QString::fromStdString(line.id));
        item.insert(QStringLiteral("title"), QString::fromStdString(line.title));
        item.insert(QStringLiteral("subtitle"), QString::fromStdString(line.subtitle));
        item.insert(QStringLiteral("quantity"), QString::fromStdString(line.quantity_text));
        item.insert(QStringLiteral("amount"), QString::fromStdString(line.amount_text));
        result.push_back(item);
    }
    return result;
}

QVariantList RecordScreenBridgeQt::history() const {
    QVariantList result;
    if (!snapshot_) return result;
    result.reserve(static_cast<qsizetype>(snapshot_->history.size()));
    for (const auto& entry : snapshot_->history) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QString::fromStdString(entry.id));
        item.insert(QStringLiteral("label"), label_from_key(entry.label_key));
        item.insert(QStringLiteral("detail"), QString::fromStdString(entry.detail_text));
        item.insert(QStringLiteral("occurredAt"),
                    QDateTime::fromMSecsSinceEpoch(entry.occurred_at_ms).toString(Qt::ISODate));
        result.push_back(item);
    }
    return result;
}

QVariantList RecordScreenBridgeQt::actions() const {
    QVariantList result;
    if (!snapshot_) return result;
    result.reserve(static_cast<qsizetype>(snapshot_->actions.size()));
    for (const auto& action : snapshot_->actions) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QString::fromStdString(action.id));
        item.insert(QStringLiteral("label"), label_from_key(action.label_key));
        result.push_back(item);
    }
    return result;
}

void RecordScreenBridgeQt::fail(QString message) {
    error_message_ = std::move(message);
    emit stateChanged();
}

bool RecordScreenBridgeQt::loadRecord(const QString& stable_id) {
    if (!loader_ || pending_) return false;
    loading_ = true;
    error_message_.clear();
    emit stateChanged();
    const auto loaded = loader_(kind_, stable_id.toStdString());
    loading_ = false;
    if (!loaded) {
        snapshot_.reset();
        emit snapshotChanged();
        fail(QString::fromStdString(loaded.error().message_key));
        return false;
    }
    snapshot_ = loaded.value();
    error_message_.clear();
    emit snapshotChanged();
    emit stateChanged();
    return true;
}

void RecordScreenBridgeQt::clearRecord() {
    snapshot_.reset();
    error_message_.clear();
    emit snapshotChanged();
    emit stateChanged();
}

void RecordScreenBridgeQt::beginCreate() {
    if (create_operation_ && !pending_) emit createRequested();
}

bool RecordScreenBridgeQt::submitCreate(const QVariantMap& values) {
    if (!create_operation_ || pending_) return false;
    return dispatch(*create_operation_, makeStableId(), values, true);
}

const app::primary::ActionSnapshot* RecordScreenBridgeQt::findAction(
    std::string_view action_id) const noexcept {
    if (!snapshot_) return nullptr;
    const auto found = std::find_if(snapshot_->actions.begin(), snapshot_->actions.end(),
                                    [action_id](const auto& action) {
                                        return action.id == action_id;
                                    });
    return found == snapshot_->actions.end() ? nullptr : &*found;
}

bool RecordScreenBridgeQt::executeAction(const QString& action_id,
                                         const QVariantMap& values) {
    if (pending_) return false;
    const auto* action = findAction(action_id.toStdString());
    if (action == nullptr || !snapshot_ || action->record_id != snapshot_->stable_id) {
        fail(QStringLiteral("record.error.action_not_available"));
        return false;
    }
    return dispatch(action->operation, QString::fromStdString(action->record_id),
                    values, true);
}

std::optional<engine::Blob> RecordScreenBridgeQt::encodeValues(
    const QVariantMap& values, QString& error_message) {
    if (values.size() > kMaximumFields) {
        error_message = QStringLiteral("record.error.too_many_fields");
        return std::nullopt;
    }
    engine::Row row;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const QByteArray key = it.key().toUtf8();
        if (!safe_key(key) || forbidden_projection_key(it.key())) {
            error_message = QStringLiteral("record.error.invalid_field");
            return std::nullopt;
        }
        const QVariant& value = it.value();
        switch (value.typeId()) {
            case QMetaType::Bool:
                row.set(key.toStdString(), engine::Value::boolean(value.toBool()));
                break;
            case QMetaType::Int:
            case QMetaType::UInt:
            case QMetaType::LongLong:
                row.set(key.toStdString(), engine::Value::integer(value.toLongLong()));
                break;
            case QMetaType::Double:
                row.set(key.toStdString(), engine::Value::real(value.toDouble()));
                break;
            case QMetaType::QString: {
                const QByteArray text = value.toString().toUtf8();
                if (text.size() > kMaximumTextBytes) {
                    error_message = QStringLiteral("record.error.field_too_long");
                    return std::nullopt;
                }
                row.set(key.toStdString(), engine::Value::text(text.toStdString()));
                break;
            }
            default:
                error_message = QStringLiteral("record.error.unsupported_field_type");
                return std::nullopt;
        }
    }
    return engine::encode_payload(row);
}

QString RecordScreenBridgeQt::makeStableId() {
    QString value = QUuid::createUuid().toString(QUuid::WithoutBraces);
    value.remove(QLatin1Char('-'));
    return value.toLower();
}

QString RecordScreenBridgeQt::makeIdempotencyKey() {
    return QStringLiteral("desktop-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool RecordScreenBridgeQt::dispatch(protocol::OperationId operation,
                                    const QString& stable_id,
                                    const QVariantMap& values, bool reload) {
    if (!dispatcher_) return false;
    QString encoding_error;
    auto payload = encodeValues(values, encoding_error);
    if (!payload) {
        fail(std::move(encoding_error));
        return false;
    }
    app::primary::CommandRequest request;
    request.operation = operation;
    request.record_id = stable_id.toStdString();
    request.payload = std::move(*payload);
    request.idempotency_key = makeIdempotencyKey().toStdString();
    pending_ = true;
    error_message_.clear();
    emit stateChanged();
    const auto result = dispatcher_(request);
    pending_ = false;
    if (!result) {
        fail(QString::fromStdString(result.error().message_key));
        return false;
    }
    emit commandAccepted(stable_id, result.value().queued, result.value().replayed);
    emit stateChanged();
    return !reload || loadRecord(stable_id);
}

}  // namespace squiflow::shell

#endif
