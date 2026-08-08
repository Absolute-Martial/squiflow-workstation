#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "app/contracts/result.hpp"
#include "app/primary/command_gateway.hpp"
#include "app/primary/primary_query.hpp"
#include "app/primary/record_query.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <optional>
#include <string_view>

namespace squiflow::shell {

// Qt projection for one authorized record screen. It never accepts an
// operation identifier from QML: lifecycle commands are resolved from the
// current authorized ActionSnapshot and create commands are fixed by PageKind.
class RecordScreenBridgeQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool pending READ pending NOTIFY stateChanged)
    Q_PROPERTY(bool hasRecord READ hasRecord NOTIFY snapshotChanged)
    Q_PROPERTY(bool canCreate READ canCreate CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY snapshotChanged)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY snapshotChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList lines READ lines NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY snapshotChanged)

  public:
    using RecordLoader = std::function<app::Result<app::primary::RecordSnapshot,
                                                   app::DomainError>(
        app::primary::PageKind, std::string_view)>;
    using CommandDispatcher = std::function<app::Result<app::primary::CommandAck,
                                                        app::DomainError>(
        const app::primary::CommandRequest&)>;

    RecordScreenBridgeQt(app::primary::PageKind kind, RecordLoader loader,
                         CommandDispatcher dispatcher,
                         std::optional<protocol::OperationId> create_operation,
                         QObject* parent = nullptr);

    bool loading() const noexcept { return loading_; }
    bool pending() const noexcept { return pending_; }
    bool hasRecord() const noexcept { return snapshot_.has_value(); }
    bool canCreate() const noexcept { return create_operation_.has_value(); }
    QString title() const;
    QString subtitle() const;
    QString errorMessage() const { return error_message_; }
    QVariantList fields() const;
    QVariantList lines() const;
    QVariantList history() const;
    QVariantList actions() const;

    Q_INVOKABLE bool loadRecord(const QString& stable_id);
    Q_INVOKABLE void clearRecord();
    Q_INVOKABLE void beginCreate();
    Q_INVOKABLE bool submitCreate(const QVariantMap& values);
    Q_INVOKABLE bool executeAction(const QString& action_id,
                                   const QVariantMap& values);

  signals:
    void snapshotChanged();
    void stateChanged();
    void createRequested();
    void commandAccepted(QString stableId, bool queued, bool replayed);

  private:
    static std::optional<engine::Blob> encodeValues(const QVariantMap& values,
                                                     QString& error_message);
    static QString makeStableId();
    static QString makeIdempotencyKey();
    bool dispatch(protocol::OperationId operation, const QString& stable_id,
                  const QVariantMap& values, bool reload);
    const app::primary::ActionSnapshot* findAction(
        std::string_view action_id) const noexcept;
    void fail(QString message);

    app::primary::PageKind kind_;
    RecordLoader loader_;
    CommandDispatcher dispatcher_;
    std::optional<protocol::OperationId> create_operation_{};
    std::optional<app::primary::RecordSnapshot> snapshot_{};
    QString error_message_{};
    bool loading_{false};
    bool pending_{false};
};

}  // namespace squiflow::shell

#endif
