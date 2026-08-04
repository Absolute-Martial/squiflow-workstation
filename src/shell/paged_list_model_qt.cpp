#include "shell/paged_list_model_qt.hpp"
#if defined(SQUIFLOW_WITH_QT)
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QString>
namespace squiflow::shell {
PagedListModelQt::PagedListModelQt(QObject*p):QAbstractListModel(p){}
int PagedListModelQt::rowCount(const QModelIndex&p)const{return p.isValid()?0:static_cast<int>(rows_.size());}
QVariant PagedListModelQt::data(const QModelIndex&i,int role)const{if(!i.isValid()||i.row()<0||static_cast<std::size_t>(i.row())>=rows_.size())return{};const auto&r=rows_[static_cast<std::size_t>(i.row())];switch(role){case StableIdRole:return QString::fromStdString(r.id);case TitleRole:return QString::fromStdString(r.title);case SubtitleRole:return QString::fromStdString(r.subtitle);default:return{};}}
QHash<int,QByteArray> PagedListModelQt::roleNames()const{return{{StableIdRole,"stableId"},{TitleRole,"title"},{SubtitleRole,"subtitle"}};}
std::uint64_t PagedListModelQt::beginQuery(){Q_ASSERT(thread()==QThread::currentThread());beginResetModel();rows_.clear();auto generation=cache_.begin_query();endResetModel();return generation;}
void PagedListModelQt::applyPage(std::uint64_t generation,std::vector<RowInput> rows){if(thread()!=QThread::currentThread()){QPointer<PagedListModelQt> self(this);QMetaObject::invokeMethod(this,[self,generation,rows=std::move(rows)]()mutable{if(self)self->applyOnGui(generation,std::move(rows));},Qt::QueuedConnection);return;}applyOnGui(generation,std::move(rows));}
void PagedListModelQt::applyOnGui(std::uint64_t generation,std::vector<RowInput> rows){Q_ASSERT(thread()==QThread::currentThread());if(!cache_.apply(generation,std::move(rows)))return;beginResetModel();rows_=cache_.snapshot();endResetModel();}
void PagedListModelQt::replaceSnapshot(std::vector<RowInput> rows){Q_ASSERT(thread()==QThread::currentThread());beginResetModel();rows_=std::move(rows);endResetModel();}
}
#endif
