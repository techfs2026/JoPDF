#ifndef THREADSAFERENDERER_H
#define THREADSAFERENDERER_H

#include <QString>
#include <QImage>
#include <QSizeF>
#include <QMutex>

extern "C" {
#include <mupdf/fitz.h>
}

/**
 * @brief 线程安全的 PDF 渲染器
 *
 * 每个实例拥有独立的 fz_context 和 fz_document
 * 适用于多线程环境，每个线程/任务创建自己的实例
 */
class ThreadSafeRenderer
{
public:
    /**
     * @brief 构造函数 - 创建渲染器并加载文档
     * @param documentPath PDF 文档路径
     */
    explicit ThreadSafeRenderer(const QString& documentPath);

    /**
     * @brief 析构函数 - 自动清理资源
     */
    ~ThreadSafeRenderer();

    // 禁止拷贝
    ThreadSafeRenderer(const ThreadSafeRenderer&) = delete;
    ThreadSafeRenderer& operator=(const ThreadSafeRenderer&) = delete;

    /**
     * @brief 检查文档是否成功加载
     */
    bool isDocumentLoaded() const;

    /**
     * @brief 获取文档总页数
     */
    int pageCount() const;

    /**
     * @brief 渲染指定页面
     * @param pageIndex 页面索引 (0-based)
     * @param zoom 缩放比例
     * @param rotation 旋转角度 (0, 90, 180, 270)
     * @return 渲染后的图像，失败返回空图像
     */
    QImage renderPage(int pageIndex, double zoom, int rotation);

    /**
     * @brief 获取页面尺寸
     * @param pageIndex 页面索引 (0-based)
     * @return 页面尺寸 (points)，失败返回空尺寸
     */
    QSizeF getPageSize(int pageIndex);

    /**
     * @brief 获取最后的错误信息
     */
    QString getLastError() const;

private:
    /**
     * @brief 初始化 MuPDF context
     * @return 成功返回 true
     */
    bool initializeContext();

    /**
     * @brief 加载 PDF 文档
     * @return 成功返回 true
     */
    bool loadDocument();

    /**
     * @brief 关闭文档
     */
    void closeDocument();

    /**
     * @brief 设置错误信息
     */
    void setLastError(const QString& error);

private:
    QString m_documentPath;        // 文档路径
    fz_context* m_context;         // MuPDF context (独立实例)
    fz_document* m_document;       // MuPDF document
    int m_pageCount;               // 文档页数
    mutable QString m_lastError;   // 最后的错误信息
    QMutex m_mutex;                // 保护并发访问
};

#endif // THREADSAFERENDERER_H
