#pragma once
#include "shell/document_snapshot.hpp"
#if defined(SQUIFLOW_WITH_QT)
#include <QByteArray>
#include <QString>
namespace squiflow::shell{class DocumentRenderQt{public:explicit DocumentRenderQt(QString prefix=QStringLiteral(":/qt/qml/SquiFlow/documents"));app::Result<QByteArray,PresentationError> render(const PreparedDocumentSnapshot&)const;app::Result<void,PresentationError> save(const PreparedDocumentSnapshot&,const QString&)const;private:QString prefix_;};}
#endif
