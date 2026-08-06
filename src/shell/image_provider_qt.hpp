#pragma once
#include "shell/thumbnail_cache.hpp"
#if defined(SQUIFLOW_WITH_QT)
#include <QQuickImageProvider>
#include <memory>
namespace squiflow::shell{struct PreviewRecord{std::string id,hash,path;};class PreviewSource{public:virtual~PreviewSource()=default;virtual std::optional<PreviewRecord> resolve(std::string_view)=0;virtual void failed(std::string_view,std::string_view)=0;};class ImageProviderQt final:public QQuickImageProvider{public:ImageProviderQt(std::shared_ptr<PreviewSource>,std::shared_ptr<ThumbnailCache>);QImage requestImage(const QString&,QSize*,const QSize&)override;static bool avif_available();private:std::shared_ptr<PreviewSource> source_;std::shared_ptr<ThumbnailCache> cache_;};}
#endif
