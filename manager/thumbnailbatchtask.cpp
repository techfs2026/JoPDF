#include "thumbnailbatchtask.h"
#include "threadsaferenderer.h"
#include "thumbnailcache.h"
#include "thumbnailmanagerv2.h"
#include <QElapsedTimer>
#include <QDebug>

ThumbnailBatchTask::ThumbnailBatchTask(const QString& docPath,
                       ThumbnailCache* cache,
                       ThumbnailManagerV2* manager,
                       const QVector<int>& pageIndices,
                       RenderPriority priority,
                       int thumbnailWidth,
                       int rotation)
    : m_renderer(std::make_unique<ThreadSafeRenderer>(docPath))
    , m_cache(cache)
    , m_manager(manager)
    , m_pageIndices(pageIndices)
    , m_priority(priority)
    , m_thumbnailWidth(thumbnailWidth)
    , m_rotation(rotation)
    , m_aborted(0)
{
    setAutoDelete(true);
}

ThumbnailBatchTask::~ThumbnailBatchTask()
{
}

void ThumbnailBatchTask::run()
{
    if (!m_renderer || !m_cache || !m_manager) {
        qWarning() << "ThumbnailBatchTask: Invalid renderer, cache or manager";
        return;
    }

    QElapsedTimer timer;
    timer.start();

    int timeBudget = getTimeBudget();
    int batchLimit = getBatchLimit();
    int rendered = 0;

    for (int pageIndex : m_pageIndices) {
        if (isAborted()) {
            qDebug() << "ThumbnailBatchTask: Aborted after rendering" << rendered << "pages";
            break;
        }

        if (!m_manager) {
            qWarning() << "ThumbnailBatchTask: Manager destroyed during rendering";
            break;
        }

        if (rendered >= batchLimit) {
            qDebug() << "ThumbnailBatchTask: Batch limit reached";
            break;
        }

        if (timer.elapsed() > timeBudget) {
            qDebug() << "ThumbnailBatchTask: Time budget exceeded:" << timer.elapsed() << "ms";
            break;
        }

        // 检查是否已缓存
        if (m_cache->has(pageIndex)) {
            continue;
        }

        // 计算缩放比例
        QSizeF pageSize = m_renderer->getPageSize(pageIndex);
        if (pageSize.isEmpty()) {
            qWarning() << "ThumbnailBatchTask: Invalid page size for page" << pageIndex;
            continue;
        }

        double zoom = m_thumbnailWidth / pageSize.width();

        // 渲染页面
        QImage thumbnail = m_renderer->renderPage(pageIndex, zoom, m_rotation);

        if (thumbnail.isNull()) {
            qWarning() << "ThumbnailBatchTask: Failed to render page" << pageIndex;
            continue;
        }

        // 保存到缓存
        m_cache->set(pageIndex, thumbnail);

        // 通知UI
        if (m_manager) {
            QMetaObject::invokeMethod(m_manager, "thumbnailLoaded",
                                      Qt::QueuedConnection,
                                      Q_ARG(int, pageIndex),
                                      Q_ARG(QImage, thumbnail));
        }

        rendered++;
    }

    qint64 elapsed = timer.elapsed();
    if (rendered > 0) {
        qDebug() << "ThumbnailBatchTask: Rendered" << rendered
                 << "pages in" << elapsed << "ms"
                 << "(" << (elapsed / rendered) << "ms/page)";
    }
}

void ThumbnailBatchTask::abort()
{
    m_aborted.storeRelaxed(1);
}

bool ThumbnailBatchTask::isAborted() const
{
    return m_aborted.loadRelaxed() != 0;
}

int ThumbnailBatchTask::getTimeBudget() const
{
    switch (m_priority) {
    case RenderPriority::IMMEDIATE:
        return 100;   // 100ms(低清渲染很快)
    case RenderPriority::HIGH:
        return 500;   // 500ms(高清可见区)
    case RenderPriority::MEDIUM:
        return 2000;  // 2s(高清预加载区)
    case RenderPriority::LOW:
        return 5000;  // 5s(低清全文档)
    }
    return 1000;
}

int ThumbnailBatchTask::getBatchLimit() const
{
    switch (m_priority) {
    case RenderPriority::IMMEDIATE:
        return 10;  // 立即渲染可见区(约 10 页)
    case RenderPriority::HIGH:
        return 10;  // 高优先级(可见区)
    case RenderPriority::MEDIUM:
        return 20;  // 中优先级(预加载区)
    case RenderPriority::LOW:
        return 50;  // 低优先级(大批量)
    }
    return 10;
}
