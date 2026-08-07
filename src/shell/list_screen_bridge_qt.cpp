#include "shell/list_screen_bridge_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include <QVariantMap>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

namespace squiflow::shell {

ListScreenBridgeQt::ListScreenBridgeQt(ListBridge& bridge, QObject* parent)
    : QObject(parent), bridge_(bridge), model_(this) {
    model_.replaceSnapshot(bridge_.cache().snapshot());
}

QVariantList ListScreenBridgeQt::columns() const {
    QVariantList result;
    for (const ListColumn& column : bridge_.columns()) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), QString::fromStdString(column.id));
        item.insert(QStringLiteral("titleKey"),
                    QString::fromStdString(column.title_key));
        item.insert(QStringLiteral("sortable"), column.sortable);
        item.insert(QStringLiteral("filterable"), column.filterable);
        result.push_back(item);
    }
    return result;
}

bool ListScreenBridgeQt::loading() const noexcept {
    return state_kind(bridge_.state()) == ViewStateKind::Loading;
}

bool ListScreenBridgeQt::hasMore() const noexcept {
    const auto* ready = std::get_if<ReadyState>(&bridge_.state());
    return ready != nullptr && ready->has_more;
}

QString ListScreenBridgeQt::errorMessage() const {
    if (!request_error_key_.isEmpty()) {
        return request_error_key_;
    }
    const auto* failed = std::get_if<FailedState>(&bridge_.state());
    return failed == nullptr ? QString{} : QString::fromStdString(failed->message_key);
}

bool ListScreenBridgeQt::emit_request(
    const app::Result<ListRequest, ListError>& result) {
    if (!result) {
        request_error_key_ = QString::fromStdString(result.error().message_key);
        emit stateChanged();
        return false;
    }
    request_error_key_.clear();
    const ListRequest& request = result.value();
    if (receivers(SIGNAL(pageRequested(qulonglong,qulonglong,qulonglong,QString,bool,QString,QString))) == 0) {
        (void)bridge_.fail(request.generation, "list.error.provider_unavailable");
        synchronize();
        return false;
    }
    emit pageRequested(
        static_cast<qulonglong>(request.generation),
        static_cast<qulonglong>(request.offset),
        static_cast<qulonglong>(request.limit),
        request.sort_field ? QString::fromStdString(*request.sort_field) : QString{},
        request.sort_direction == SortDirection::Descending,
        request.filter_field ? QString::fromStdString(*request.filter_field) : QString{},
        QString::fromStdString(request.filter_text));
    emit stateChanged();
    return true;
}

bool ListScreenBridgeQt::refresh(const QString& sort_field, bool descending,
                                 const QString& filter_field,
                                 const QString& filter_text) {
    const std::optional<std::string> sort = sort_field.isEmpty()
        ? std::optional<std::string>{}
        : std::optional<std::string>{sort_field.toStdString()};
    const std::optional<std::string> filter = filter_field.isEmpty()
        ? std::optional<std::string>{}
        : std::optional<std::string>{filter_field.toStdString()};
    const auto request = bridge_.begin_refresh(
        sort, descending ? SortDirection::Descending : SortDirection::Ascending,
        filter, filter_text.toStdString());
    if (request) {
        model_.replaceSnapshot({});
    }
    return emit_request(request);
}

bool ListScreenBridgeQt::nextPage() {
    return emit_request(bridge_.next_page());
}

bool ListScreenBridgeQt::selectRow(const QString& stable_id) {
    const auto selected = bridge_.select(stable_id.toStdString());
    if (!selected) {
        emit stateChanged();
        return false;
    }
    emit rowActivated(stable_id);
    return true;
}

void ListScreenBridgeQt::synchronize() {
    model_.replaceSnapshot(bridge_.cache().snapshot());
    emit stateChanged();
}

void ListScreenBridgeQt::applyPage(std::uint64_t generation,
                                   std::vector<RowInput> rows, bool has_more) {
    if (thread() != QThread::currentThread()) {
        QPointer<ListScreenBridgeQt> self(this);
        QMetaObject::invokeMethod(
            this,
            [self, generation, rows = std::move(rows), has_more]() mutable {
                if (self) {
                    self->applyPageOnGui(generation, std::move(rows), has_more);
                }
            },
            Qt::QueuedConnection);
        return;
    }
    applyPageOnGui(generation, std::move(rows), has_more);
}

void ListScreenBridgeQt::applyPageOnGui(std::uint64_t generation,
                                        std::vector<RowInput> rows, bool has_more) {
    Q_ASSERT(thread() == QThread::currentThread());
    if (bridge_.apply_page(generation, std::move(rows), has_more)) {
        request_error_key_.clear();
        synchronize();
    }
}

void ListScreenBridgeQt::failPage(std::uint64_t generation,
                                  std::string message_key) {
    if (thread() != QThread::currentThread()) {
        QPointer<ListScreenBridgeQt> self(this);
        QMetaObject::invokeMethod(
            this,
            [self, generation, message_key = std::move(message_key)]() mutable {
                if (self) {
                    self->failPageOnGui(generation, std::move(message_key));
                }
            },
            Qt::QueuedConnection);
        return;
    }
    failPageOnGui(generation, std::move(message_key));
}

void ListScreenBridgeQt::failPageOnGui(std::uint64_t generation,
                                       std::string message_key) {
    Q_ASSERT(thread() == QThread::currentThread());
    if (bridge_.fail(generation, std::move(message_key))) {
        request_error_key_.clear();
        synchronize();
    }
}

void ListScreenBridgeQt::cancel() noexcept {
    bridge_.cancel();
    request_error_key_.clear();
    synchronize();
}

}  // namespace squiflow::shell

#endif
