#ifndef THREADSAFERENDERER_H
#define THREADSAFERENDERER_H

#include <QString>
#include <QImage>
#include <QSizeF>
#include <QMutex>
#include <QThreadStorage>
#include <QThread>

extern "C" {
#include <mupdf/fitz.h>
}

/**
 * @brief 线程安全的 MuPDF 渲染器 (修复版)
 *
 * 核心设计变更：
 * - 每个线程维护自己的 fz_context
 * - 每个线程维护自己的 fz_document（独立打开）
 * - 使用 QThreadStorage 实现 TLS
 *
 * 为什么不共享 document？
 * - MuPDF 的 fz_document 内部有状态（缓存、页面树等）
 * - 多线程访问即使只读也可能导致数据竞争
 * - 每个线程独立打开文档是最安全的方案
 */
class ThreadSafeRenderer
{
public:
    /**
     * @brief 构造函数
     * @param documentPath PDF 文档路径
     */
    explicit ThreadSafeRenderer(const QString& documentPath);

    ~ThreadSafeRenderer();

    /**
     * @brief 渲染页面（线程安全）
     * @param pageIndex 页面索引
     * @param zoom 缩放比例
     * @param rotation 旋转角度
     * @return 渲染的图片，失败返回空图片
     */
    QImage renderPage(int pageIndex, double zoom, int rotation = 0);

    /**
     * @brief 获取页面尺寸（线程安全）
     */
    QSizeF getPageSize(int pageIndex);

    /**
     * @brief 获取页数
     */
    int pageCount() const { return m_pageCount; }

    /**
     * @brief 是否初始化成功
     */
    bool isValid() const { return !m_documentPath.isEmpty(); }

private:
    // 文档路径（所有线程共享）
    QString m_documentPath;
    int m_pageCount;  // 页数（只初始化一次）

    // 线程局部数据
    QThreadStorage<fz_context*> m_threadContexts;
    QThreadStorage<fz_document*> m_threadDocuments;

    // 保护共享状态
    mutable QMutex m_cleanupMutex;

    // 获取当前线程的 context
    fz_context* getThreadContext();

    // 获取当前线程的 document（每个线程独立打开）
    fz_document* getThreadDocument(fz_context* ctx);
};

#endif // THREADSAFERENDERER_H
