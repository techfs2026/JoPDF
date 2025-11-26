#ifndef THUMBNAILBATCHTASK_H
#define THUMBNAILBATCHTASK_H

#include <QRunnable>
#include <QVector>
#include <QAtomicInt>
#include <QImage>
#include <QPointer>

class ThreadSafeRenderer;
class ThumbnailCache;
class ThumbnailManagerV2;

/**
 * @brief 渲染优先级
 */
enum class RenderPriority {
    IMMEDIATE,    // 立即渲染（低清，同步）
    HIGH,         // 高优先级（高清，可见区）
    MEDIUM,       // 中优先级（高清，预加载区）
    LOW           // 低优先级（低清，全文档）
};

class ThumbnailBatchTask : public QRunnable
{
public:
    ThumbnailBatchTask(const QString& docPath,
                       ThumbnailCache* cache,
                       ThumbnailManagerV2* manager,
                       const QVector<int>& pageIndices,
                       RenderPriority priority,
                       int thumbnailWidth,
                       int rotation);

    ~ThumbnailBatchTask();

    void run() override;
    void abort();
    bool isAborted() const;

private:
    int getTimeBudget() const;
    int getBatchLimit() const;

    std::unique_ptr<ThreadSafeRenderer> m_renderer;
    ThumbnailCache* m_cache;
    ThumbnailManagerV2* m_manager;
    QVector<int> m_pageIndices;
    RenderPriority m_priority;
    int m_thumbnailWidth;
    int m_rotation;
    QAtomicInt m_aborted;
};

#endif // THUMBNAILBATCHTASK_H
