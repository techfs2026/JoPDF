#ifndef THUMBNAILCACHE_H
#define THUMBNAILCACHE_H

#include <QImage>
#include <QMap>
#include <QReadWriteLock>
#include <QMutex>

/**
 * @brief 双层缩略图缓存（终身缓存，不淘汰）
 *
 * 架构：
 * - 低清缓存：40px 宽，全文档缓存（约 5KB/页）
 * - 高清缓存：120px 宽，全文档缓存（约 150KB/页）
 *
 * 线程安全：使用读写锁保护
 */
class ThumbnailCache
{
public:
    explicit ThumbnailCache();
    ~ThumbnailCache();

    // ========== 低清缓存 ==========

    /**
     * @brief 获取低清缩略图
     * @return 如果不存在返回空图片
     */
    QImage getLowRes(int pageIndex) const;

    /**
     * @brief 设置低清缩略图
     */
    void setLowRes(int pageIndex, const QImage& thumbnail);

    /**
     * @brief 检查低清缓存是否存在
     */
    bool hasLowRes(int pageIndex) const;

    // ========== 高清缓存 ==========

    /**
     * @brief 获取高清缩略图
     * @return 如果不存在返回空图片
     */
    QImage getHighRes(int pageIndex) const;

    /**
     * @brief 设置高清缓存
     */
    void setHighRes(int pageIndex, const QImage& thumbnail);

    /**
     * @brief 检查高清缓存是否存在
     */
    bool hasHighRes(int pageIndex) const;

    // ========== 管理方法 ==========

    /**
     * @brief 清空所有缓存
     */
    void clear();

    /**
     * @brief 获取统计信息
     */
    QString getStatistics() const;

    /**
     * @brief 获取已缓存的低清数量
     */
    int lowResCount() const;

    /**
     * @brief 获取已缓存的高清数量
     */
    int highResCount() const;

private:
    mutable QReadWriteLock m_lowResLock;
    mutable QReadWriteLock m_highResLock;

    QMap<int, QImage> m_lowResCache;   // 低清缓存（终身）
    QMap<int, QImage> m_highResCache;  // 高清缓存（终身）
};

#endif // THUMBNAILCACHE_H
