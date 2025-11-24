#include "thumbnailcache.h"
#include <QReadLocker>
#include <QWriteLocker>

ThumbnailCache::ThumbnailCache()
{
}

ThumbnailCache::~ThumbnailCache()
{
}

// ========== 低清缓存 ==========

QImage ThumbnailCache::getLowRes(int pageIndex) const
{
    QReadLocker locker(&m_lowResLock);
    return m_lowResCache.value(pageIndex, QImage());
}

void ThumbnailCache::setLowRes(int pageIndex, const QImage& thumbnail)
{
    if (thumbnail.isNull()) {
        return;
    }

    QWriteLocker locker(&m_lowResLock);
    m_lowResCache[pageIndex] = thumbnail;
}

bool ThumbnailCache::hasLowRes(int pageIndex) const
{
    QReadLocker locker(&m_lowResLock);
    return m_lowResCache.contains(pageIndex);
}

// ========== 高清缓存 ==========

QImage ThumbnailCache::getHighRes(int pageIndex) const
{
    QReadLocker locker(&m_highResLock);
    return m_highResCache.value(pageIndex, QImage());
}

void ThumbnailCache::setHighRes(int pageIndex, const QImage& thumbnail)
{
    if (thumbnail.isNull()) {
        return;
    }

    QWriteLocker locker(&m_highResLock);
    m_highResCache[pageIndex] = thumbnail;
}

bool ThumbnailCache::hasHighRes(int pageIndex) const
{
    QReadLocker locker(&m_highResLock);
    return m_highResCache.contains(pageIndex);
}

// ========== 管理方法 ==========

void ThumbnailCache::clear()
{
    {
        QWriteLocker locker(&m_lowResLock);
        m_lowResCache.clear();
    }

    {
        QWriteLocker locker(&m_highResLock);
        m_highResCache.clear();
    }
}

QString ThumbnailCache::getStatistics() const
{
    int lowCount = lowResCount();
    int highCount = highResCount();

    // 估算内存占用（低清 5KB/页，高清 150KB/页）
    qint64 lowMemory = lowCount * 5;      // KB
    qint64 highMemory = highCount * 150;   // KB
    qint64 totalMemory = lowMemory + highMemory;

    return QString("Thumbnail Cache: Low=%1 (%.2 MB), High=%3 (%.4 MB), Total=%.5 MB")
        .arg(lowCount)
        .arg(lowMemory / 1024.0, 0, 'f', 2)
        .arg(highCount)
        .arg(highMemory / 1024.0, 0, 'f', 2)
        .arg(totalMemory / 1024.0, 0, 'f', 2);
}

int ThumbnailCache::lowResCount() const
{
    QReadLocker locker(&m_lowResLock);
    return m_lowResCache.size();
}

int ThumbnailCache::highResCount() const
{
    QReadLocker locker(&m_highResLock);
    return m_highResCache.size();
}
