#pragma once
#if defined(SQUIFLOW_WITH_QT)
#include "shell/paged_list_cache.hpp"
#include <QAbstractListModel>
#include <QHash>
#include <QByteArray>
#include <cstdint>
#include <vector>
namespace squiflow::shell {
class PagedListModelQt final:public QAbstractListModel{public:enum Role{StableIdRole=Qt::UserRole+1,TitleRole,SubtitleRole};explicit PagedListModelQt(QObject* parent=nullptr);int rowCount(const QModelIndex& parent={})const override;QVariant data(const QModelIndex& index,int role)const override;QHash<int,QByteArray> roleNames()const override;std::uint64_t beginQuery();void applyPage(std::uint64_t generation,std::vector<RowInput> rows);void replaceSnapshot(std::vector<RowInput> rows);private:void applyOnGui(std::uint64_t,std::vector<RowInput>);PagedListCache cache_;std::vector<RowInput> rows_;};
}
#endif
