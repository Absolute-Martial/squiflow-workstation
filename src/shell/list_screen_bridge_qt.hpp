#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "shell/list_bridge.hpp"
#include "shell/paged_list_model_qt.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace squiflow::shell {

class ListScreenBridgeQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* model READ model CONSTANT)
    Q_PROPERTY(QVariantList columns READ columns CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)

  public:
    explicit ListScreenBridgeQt(ListBridge& bridge, QObject* parent = nullptr);

    QAbstractItemModel* model() noexcept { return &model_; }
    QVariantList columns() const;
    bool loading() const noexcept;
    bool hasMore() const noexcept;
    QString errorMessage() const;

    Q_INVOKABLE bool refresh(const QString& sort_field = {}, bool descending = false,
                             const QString& filter_field = {},
                             const QString& filter_text = {});
    Q_INVOKABLE bool nextPage();
    Q_INVOKABLE bool selectRow(const QString& stable_id);
    void applyPage(std::uint64_t generation, std::vector<RowInput> rows,
                   bool has_more);
    void failPage(std::uint64_t generation, std::string message_key);
    void cancel() noexcept;

  signals:
    void pageRequested(qulonglong generation, qulonglong offset, qulonglong limit,
                       QString sortField, bool descending, QString filterField,
                       QString filterText);
    void rowActivated(QString stableId);
    void stateChanged();

  private:
    bool emit_request(const app::Result<ListRequest, ListError>& request);
    void synchronize();
    void applyPageOnGui(std::uint64_t generation, std::vector<RowInput> rows,
                        bool has_more);
    void failPageOnGui(std::uint64_t generation, std::string message_key);

    ListBridge& bridge_;
    PagedListModelQt model_;
    QString request_error_key_{};
};

}  // namespace squiflow::shell

#endif
