#ifndef THUMBNAILCACHE_H
#define THUMBNAILCACHE_H

#include <QImage>
#include <QHash>
#include <QReadWriteLock>

class ThumbnailCache
{
public:
    ThumbnailCache();
    ~ThumbnailCache();

    // 缩略图缓存（移除高清/低清区分，统一使用单一缓存）
    QImage get(int pageIndex) const;
    void set(int pageIndex, const QImage& thumbnail);
    bool has(int pageIndex) const;

    // 管理
    void clear();
    QString getStatistics() const;
    int count() const;

private:
    QHash<int, QImage> m_cache;
    mutable QReadWriteLock m_lock;
};

#endif // THUMBNAILCACHE_H
