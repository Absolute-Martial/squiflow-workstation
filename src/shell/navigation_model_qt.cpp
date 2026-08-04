#include "shell/navigation_model_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include <QThread>
#include <QUrl>
#include <QVariant>

namespace squiflow::shell {

NavigationModelQt::NavigationModelQt(NavigationController& controller, QObject* parent)
    : QAbstractListModel(parent), controller_(controller) {}

int NavigationModelQt::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant NavigationModelQt::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        static_cast<std::size_t>(index.row()) >= rows_.size()) {
        return {};
    }
    const NavigationRow& row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case StableIdRole: return QString::fromStdString(row.stable_id);
    case TitleKeyRole: return QString::fromStdString(row.title_key);
    case IconNameRole: return QString::fromStdString(row.icon_name);
    case ComponentUrlRole: return QUrl(QString::fromStdString(row.component_url));
    case GroupKeyRole: return QString::fromStdString(row.group_key);
    case GroupRankRole: return static_cast<int>(row.group_rank);
    case ScreenRankRole: return static_cast<int>(row.screen_rank);
    case SelectedRole: return row.selected;
    case ModuleIdRole: return static_cast<int>(row.owner);
    default: return {};
    }
}

QHash<int, QByteArray> NavigationModelQt::roleNames() const {
    return {{StableIdRole, "stableId"},
            {TitleKeyRole, "titleKey"},
            {IconNameRole, "iconName"},
            {ComponentUrlRole, "componentUrl"},
            {GroupKeyRole, "groupKey"},
            {GroupRankRole, "groupRank"},
            {ScreenRankRole, "screenRank"},
            {SelectedRole, "selected"},
            {ModuleIdRole, "moduleId"}};
}

void NavigationModelQt::refreshFromController() {
    Q_ASSERT(thread() == QThread::currentThread());
    beginResetModel();
    rows_ = controller_.rows();
    endResetModel();
}

}  // namespace squiflow::shell

#endif
