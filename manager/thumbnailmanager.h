#ifndef THUMBNAILMANAGER_NEW_H
#define THUMBNAILMANAGER_NEW_H

#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <QVector>
#include <QSet>
#include <memory>

#include "thumbnailbatchtask.h"

class MuPDFRenderer;
class ThreadSafeRenderer;
class ThumbnailCache;

/**
 * @brief 智能缩略图管理器（双分辨率 + 优先级调度）
 *
 * 特点：
 * 1. 双分辨率缓存（低清 40px + 高清 120px）
 * 2. 四级优先级队列（IMMEDIATE > HIGH > MEDIUM > LOW）
 * 3. 任务中断与取消
 * 4. 线程池管理
 * 5. 终身缓存（不淘汰）
 * 6. 线程安全渲染
 */
class ThumbnailManager : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailManager(MuPDFRenderer* renderer, QObject* parent = nullptr);
    ~ThumbnailManager();

    // ========== 配置 ==========

    /**
     * @brief 设置低清宽度（默认 40px）
     */
    void setLowResWidth(int width);

    /**
     * @brief 设置高清宽度（默认 120px）
     */
    void setHighResWidth(int width);

    /**
     * @brief 设置旋转角度
     */
    void setRotation(int rotation);

    // ========== 获取缩略图 ==========

    /**
     * @brief 获取缩略图（优先返回高清，其次低清）
     * @param pageIndex 页面索引
     * @param preferHighRes 是否优先高清
     * @return 如果都不存在返回空图片
     */
    QImage getThumbnail(int pageIndex, bool preferHighRes = true);

    /**
     * @brief 检查是否有缩略图（低清或高清）
     */
    bool hasThumbnail(int pageIndex) const;

    // ========== 渲染请求 ==========

    /**
     * @brief 立即渲染低清缩略图（同步，阻塞调用者）
     * @param pageIndices 页面索引列表
     * @note 用于首次打开文档，快速显示可见区
     */
    void renderLowResImmediate(const QVector<int>& pageIndices);

    /**
     * @brief 异步渲染高清缩略图（高优先级，可见区）
     */
    void renderHighResAsync(const QVector<int>& pageIndices, RenderPriority priority);

    /**
     * @brief 异步渲染低清缩略图（低优先级，全文档）
     */
    void renderLowResAsync(const QVector<int>& pageIndices);

    // ========== 任务控制 ==========

    /**
     * @brief 取消所有进行中的任务
     */
    void cancelAllTasks();

    /**
     * @brief 取消低优先级任务
     */
    void cancelLowPriorityTasks();

    /**
     * @brief 等待所有任务完成
     */
    void waitForCompletion();

    // ========== 管理 ==========

    /**
     * @brief 清空缓存
     */
    void clear();

    /**
     * @brief 获取统计信息
     */
    QString getStatistics() const;

    /**
     * @brief 获取已缓存数量
     */
    int cachedCount() const;

signals:
    /**
     * @brief 缩略图已加载（低清或高清）
     * @param pageIndex 页面索引
     * @param thumbnail 缩略图
     * @param isHighRes 是否高清
     */
    void thumbnailLoaded(int pageIndex, const QImage& thumbnail, bool isHighRes);

    /**
     * @brief 批量加载进度
     * @param loaded 已加载数量
     * @param total 总数量
     */
    void loadProgress(int loaded, int total);

private:
    MuPDFRenderer* m_renderer;  // UI 线程使用
    std::unique_ptr<ThreadSafeRenderer> m_threadSafeRenderer;  // 工作线程使用
    std::unique_ptr<ThumbnailCache> m_cache;
    std::unique_ptr<QThreadPool> m_threadPool;

    int m_lowResWidth;
    int m_highResWidth;
    int m_rotation;

    // 任务跟踪
    QMutex m_taskMutex;
    QVector<ThumbnailBatchTask*> m_activeTasks;

    // 添加任务到跟踪列表
    void trackTask(ThumbnailBatchTask* task);

    // 移除任务（任务完成时自动调用）
    void untrackTask(ThumbnailBatchTask* task);
};

#endif // THUMBNAILMANAGER_NEW_H
