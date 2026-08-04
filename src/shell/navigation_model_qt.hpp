#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "shell/navigation_controller.hpp"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>

#include <vector>

namespace squiflow::shell {

class NavigationModelQt final : public QAbstractListModel {
    Q_OBJECT

  public:
    enum Role {
        StableIdRole = Qt::UserRole + 1,
        TitleKeyRole,
        IconNameRole,
        ComponentUrlRole,
        GroupKeyRole,
        GroupRankRole,
        ScreenRankRole,
        SelectedRole,
        ModuleIdRole,
    };

    explicit NavigationModelQt(NavigationController& controller,
                               QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void refreshFromController();

  private:
    NavigationController& controller_;
    std::vector<NavigationRow> rows_{};
};

}  // namespace squiflow::shell

#endif
