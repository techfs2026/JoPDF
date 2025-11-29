#ifndef THUMBNAILMANAGER_V2_H
#define THUMBNAILMANAGER_V2_H

#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <QTimer>
#include <memory>

#include "thumbnailbatchtask.h"
#include "thumbnailloadstrategy.h"

class MuPDFRenderer;
class ThumbnailCache;

/**
 * @brief 智能缩略图管理器 V2 - 简化版
 *
 * 加载策略:
 * - 小文档(<50页): 全量同步加载
 * - 中文档(50-200页): 可见区同步 + 后台批次异步
 * - 大文档(>200页): 仅按需同步加载(滚动停止时)
 */
class ThumbnailManagerV2 : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailManagerV2(MuPDFRenderer* renderer, QObject* parent = nullptr);
    ~ThumbnailManagerV2();

    // ========== 配置 ==========
    void setThumbnailWidth(int width);
    void setRotation(int rotation);

    // ========== 获取缩略图 ==========
    QImage getThumbnail(int pageIndex) const;
    bool hasThumbnail(int pageIndex) const;

    // ========== 加载控制 ==========

    /**
     * @brief 启动缩略图加载（根据策略自动选择）
     * @param initialVisible 初始可见页面
     */
    void startLoading(const QSet<int>& initialVisible);

    /**
     * @brief 同步加载指定页面（主要用于大文档滚动停止后）
     * @param pages 需要加载的页面索引
     */
    void syncLoadPages(const QVector<int>& pages);

    /**
     * @brief 处理慢速滚动（大文档专用）
     * @param visiblePages 当前可见的页面索引
     *
     * 当检测到用户慢速滚动时调用，同步加载可见区域。
     * 仅对大文档生效，快速滚动时不会调用此方法。
     */
    void handleSlowScroll(const QSet<int>& visiblePages);

    /**
     * @brief 取消所有后台任务（仅中文档使用）
     */
    void cancelAllTasks();

    /**
     * @brief 等待所有任务完成
     */
    void waitForCompletion();

    // ========== 管理 ==========
    void clear();
    QString getStatistics() const;
    int cachedCount() const;

    /**
     * @brief 是否应该响应滚动事件
     * @return 仅大文档且未在批次加载中时返回false
     */
    bool shouldRespondToScroll() const;

signals:
    void thumbnailLoaded(int pageIndex, const QImage& thumbnail);
    void loadProgress(int loaded, int total);
    void batchCompleted(int batchIndex, int totalBatches);
    void allCompleted();

    void loadingStarted(int totalPages, const QString& strategy);
    void loadingStatusChanged(const QString& status);

private slots:
    void processNextBatch();

private:
    // 同步渲染（小文档、中文档初始可见区、大文档按需加载）
    void renderPagesSync(const QVector<int>& pages);

    // 异步渲染（仅中文档后台批次使用）
    void renderPagesAsync(const QVector<int>& pages, RenderPriority priority);

    // 设置中文档后台批次
    void setupBackgroundBatches();

    void trackTask(ThumbnailBatchTask* task);

private:
    MuPDFRenderer* m_renderer;
    std::unique_ptr<ThumbnailCache> m_cache;
    std::unique_ptr<QThreadPool> m_threadPool;
    std::unique_ptr<ThumbnailLoadStrategy> m_strategy;

    int m_thumbnailWidth;
    int m_rotation;

    // 批次管理（仅中文档使用）
    QVector<QVector<int>> m_backgroundBatches;
    int m_currentBatchIndex;
    QTimer* m_batchTimer;

    // 任务跟踪（仅中文档使用）
    QMutex m_taskMutex;
    QVector<ThumbnailBatchTask*> m_activeTasks;

    bool m_isLoadingInProgress;  // 标记中文档是否正在批次加载中
};

#endif // THUMBNAILMANAGER_V2_H
