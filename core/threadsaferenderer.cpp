#include "threadsaferenderer.h"
#include <QDebug>
#include <cstring>
#include "mupdfrendererutil.h"

ThreadSafeRenderer::ThreadSafeRenderer(const QString& documentPath)
    : m_documentPath(documentPath)
    , m_pageCount(0)
{
    qInfo() << "ThreadSafeRenderer: Initialized for" << documentPath;
}

ThreadSafeRenderer::~ThreadSafeRenderer()
{
    QMutexLocker locker(&m_cleanupMutex);

    // QThreadStorage 会在线程结束时自动清理 context 和 document
    // 我们需要手动清理当前线程的资源
    if (m_threadDocuments.hasLocalData()) {
        fz_document* doc = m_threadDocuments.localData();
        if (doc) {
            fz_context* ctx = m_threadContexts.hasLocalData() ?
                                  m_threadContexts.localData() : nullptr;
            if (ctx) {
                fz_drop_document(ctx, doc);
            }
        }
    }

    if (m_threadContexts.hasLocalData()) {
        fz_context* ctx = m_threadContexts.localData();
        if (ctx) {
            fz_drop_context(ctx);
        }
    }

    qInfo() << "ThreadSafeRenderer: Destroyed";
}

fz_context* ThreadSafeRenderer::getThreadContext()
{
    if (!m_threadContexts.hasLocalData()) {
        // 为当前线程创建新的 context
        fz_locks_context locks;
        locks.user = nullptr;
        locks.lock = lock_mutex;
        locks.unlock = unlock_mutex;

        fz_context* ctx = fz_new_context(nullptr, &locks, FZ_STORE_DEFAULT);
        if (!ctx) {
            qCritical() << "ThreadSafeRenderer: Failed to create thread context";
            return nullptr;
        }

        // 注册文档处理器
        fz_try(ctx) {
            fz_register_document_handlers(ctx);
        }
        fz_catch(ctx) {
            qCritical() << "ThreadSafeRenderer: Failed to register handlers in thread";
            fz_drop_context(ctx);
            return nullptr;
        }

        m_threadContexts.setLocalData(ctx);

        qDebug() << "ThreadSafeRenderer: Created context for thread"
                 << QThread::currentThreadId();
    }

    return m_threadContexts.localData();
}

fz_document* ThreadSafeRenderer::getThreadDocument(fz_context* ctx)
{
    if (!ctx) {
        return nullptr;
    }

    if (!m_threadDocuments.hasLocalData()) {
        // 为当前线程打开文档
        fz_document* doc = nullptr;

        QByteArray pathUtf8 = m_documentPath.toUtf8();
        fz_try(ctx) {
            doc = fz_open_document(ctx, pathUtf8.constData());

            // 只在第一次打开时记录页数
            if (m_pageCount == 0) {
                QMutexLocker locker(&m_cleanupMutex);
                if (m_pageCount == 0) {  // 双重检查
                    m_pageCount = fz_count_pages(ctx, doc);
                    qInfo() << "ThreadSafeRenderer: Document has" << m_pageCount << "pages";
                }
            }
        }
        fz_catch(ctx) {
            qCritical() << "ThreadSafeRenderer: Failed to open document:"
                        << fz_caught_message(ctx);
            return nullptr;
        }

        m_threadDocuments.setLocalData(doc);

        qDebug() << "ThreadSafeRenderer: Opened document for thread"
                 << QThread::currentThreadId();
    }

    return m_threadDocuments.localData();
}

QImage ThreadSafeRenderer::renderPage(int pageIndex, double zoom, int rotation)
{
    if (pageIndex < 0 || (m_pageCount > 0 && pageIndex >= m_pageCount)) {
        qWarning() << "ThreadSafeRenderer: Invalid page index:" << pageIndex;
        return QImage();
    }

    fz_context* ctx = getThreadContext();
    if (!ctx) {
        return QImage();
    }

    fz_document* doc = getThreadDocument(ctx);
    if (!doc) {
        return QImage();
    }

    QImage result;

    fz_try(ctx) {
        // 加载页面
        fz_page* page = fz_load_page(ctx, doc, pageIndex);

        // 计算变换矩阵
        fz_matrix matrix = fz_scale(zoom, zoom);
        int normalizedRotation = rotation % 360;
        if (normalizedRotation < 0) normalizedRotation += 360;
        if (normalizedRotation != 0) {
            matrix = fz_concat(matrix, fz_rotate(normalizedRotation));
        }

        // 计算边界
        fz_rect bounds = fz_bound_page(ctx, page);
        bounds = fz_transform_rect(bounds, matrix);

        // 创建 pixmap
        fz_pixmap* pixmap = fz_new_pixmap_with_bbox(
            ctx,
            fz_device_rgb(ctx),
            fz_round_rect(bounds),
            nullptr,
            0
            );
        fz_clear_pixmap_with_value(ctx, pixmap, 0xff);

        // 渲染
        fz_device* device = fz_new_draw_device(ctx, fz_identity, pixmap);
        fz_run_page(ctx, page, device, matrix, nullptr);

        // 转换为 QImage
        int width = fz_pixmap_width(ctx, pixmap);
        int height = fz_pixmap_height(ctx, pixmap);
        int stride = fz_pixmap_stride(ctx, pixmap);
        unsigned char* samples = fz_pixmap_samples(ctx, pixmap);

        result = QImage(width, height, QImage::Format_RGB888);
        for (int y = 0; y < height; ++y) {
            unsigned char* src = samples + y * stride;
            unsigned char* dest = result.scanLine(y);
            memcpy(dest, src, width * 3);
        }

        // 清理
        fz_close_device(ctx, device);
        fz_drop_device(ctx, device);
        fz_drop_pixmap(ctx, pixmap);
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx) {
        qWarning() << "ThreadSafeRenderer: Failed to render page" << pageIndex
                   << ":" << fz_caught_message(ctx);
        result = QImage();
    }

    return result;
}

QSizeF ThreadSafeRenderer::getPageSize(int pageIndex)
{
    if (pageIndex < 0 || (m_pageCount > 0 && pageIndex >= m_pageCount)) {
        return QSizeF();
    }

    fz_context* ctx = getThreadContext();
    if (!ctx) {
        return QSizeF();
    }

    fz_document* doc = getThreadDocument(ctx);
    if (!doc) {
        return QSizeF();
    }

    QSizeF size;

    fz_try(ctx) {
        fz_page* page = fz_load_page(ctx, doc, pageIndex);
        fz_rect bounds = fz_bound_page(ctx, page);
        size.setWidth(bounds.x1 - bounds.x0);
        size.setHeight(bounds.y1 - bounds.y0);
        fz_drop_page(ctx, page);
    }
    fz_catch(ctx) {
        qWarning() << "ThreadSafeRenderer: Failed to get page size" << pageIndex;
        size = QSizeF();
    }

    return size;
}
