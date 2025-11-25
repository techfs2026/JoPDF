#include "threadsaferenderer.h"
#include <QDebug>
#include <cstring>
#include <QThread>
#include "mupdfrendererutil.h"

// ========================================
// ThreadSafeRenderer 实现
// 每个实例拥有独立的 context 和 document
// ========================================

ThreadSafeRenderer::ThreadSafeRenderer(const QString& documentPath)
    : m_documentPath(documentPath)
    , m_context(nullptr)
    , m_document(nullptr)
    , m_pageCount(0)
{
    qDebug() << "ThreadSafeRenderer: Creating for" << documentPath
             << "Thread:" << QThread::currentThreadId();

    // 初始化 context
    if (!initializeContext()) {
        qCritical() << "ThreadSafeRenderer: Failed to initialize context";
        return;
    }

    // 加载文档
    if (!loadDocument()) {
        qCritical() << "ThreadSafeRenderer: Failed to load document";
        return;
    }

    qInfo() << "ThreadSafeRenderer: Successfully initialized with"
            << m_pageCount << "pages"
            << "Thread:" << QThread::currentThreadId();
}

ThreadSafeRenderer::~ThreadSafeRenderer()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "ThreadSafeRenderer: Destroying"
             << "Thread:" << QThread::currentThreadId();

    // 关闭文档
    closeDocument();

    // 销毁 context
    if (m_context) {
        fz_drop_context(m_context);
        m_context = nullptr;
    }

    qDebug() << "ThreadSafeRenderer: Destroyed"
             << "Thread:" << QThread::currentThreadId();
}

bool ThreadSafeRenderer::initializeContext()
{

    m_context = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);

    if (!m_context) {
        setLastError("Failed to create MuPDF context");
        qCritical() << "ThreadSafeRenderer: Failed to create context";
        return false;
    }

    // 注册文档处理器
    fz_try(m_context) {
        fz_register_document_handlers(m_context);
    }
    fz_catch(m_context) {
        QString err = QString("Failed to register document handlers: %1")
        .arg(fz_caught_message(m_context));
        setLastError(err);
        qCritical() << "ThreadSafeRenderer:" << err;
        fz_drop_context(m_context);
        m_context = nullptr;
        return false;
    }

    return true;
}

bool ThreadSafeRenderer::loadDocument()
{
    if (!m_context) {
        setLastError("Invalid MuPDF context");
        return false;
    }

    QByteArray pathUtf8 = m_documentPath.toUtf8();

    fz_try(m_context) {
        m_document = fz_open_document(m_context, pathUtf8.constData());
        m_pageCount = fz_count_pages(m_context, m_document);

        qInfo() << "ThreadSafeRenderer: Loaded document with"
                << m_pageCount << "pages";
    }
    fz_catch(m_context) {
        QString err = QString("Failed to open document: %1")
        .arg(fz_caught_message(m_context));
        setLastError(err);
        qCritical() << "ThreadSafeRenderer:" << err;
        m_document = nullptr;
        m_pageCount = 0;
        return false;
    }

    return true;
}

void ThreadSafeRenderer::closeDocument()
{
    if (m_document && m_context) {
        fz_drop_document(m_context, m_document);
        m_document = nullptr;
        m_pageCount = 0;
    }
}

bool ThreadSafeRenderer::isDocumentLoaded() const
{
    return m_document != nullptr;
}

int ThreadSafeRenderer::pageCount() const
{
    return m_pageCount;
}

QImage ThreadSafeRenderer::renderPage(int pageIndex, double zoom, int rotation)
{
    QMutexLocker locker(&m_mutex);

    if (!m_document || !m_context) {
        qWarning() << "ThreadSafeRenderer: No document loaded";
        return QImage();
    }

    if (pageIndex < 0 || pageIndex >= m_pageCount) {
        qWarning() << "ThreadSafeRenderer: Invalid page index:" << pageIndex;
        return QImage();
    }

    QImage result;

    fz_try(m_context) {
        // 加载页面
        fz_page* page = fz_load_page(m_context, m_document, pageIndex);

        // 计算变换矩阵
        fz_matrix matrix = fz_scale(zoom, zoom);
        int normalizedRotation = rotation % 360;
        if (normalizedRotation < 0) normalizedRotation += 360;
        if (normalizedRotation != 0) {
            matrix = fz_concat(matrix, fz_rotate(normalizedRotation));
        }

        // 计算边界
        fz_rect bounds = fz_bound_page(m_context, page);
        bounds = fz_transform_rect(bounds, matrix);

        // 创建 pixmap
        fz_pixmap* pixmap = fz_new_pixmap_with_bbox(
            m_context,
            fz_device_rgb(m_context),
            fz_round_rect(bounds),
            nullptr,
            0
            );
        fz_clear_pixmap_with_value(m_context, pixmap, 0xff);

        // 渲染
        fz_device* device = fz_new_draw_device(m_context, fz_identity, pixmap);
        fz_run_page(m_context, page, device, matrix, nullptr);

        // 转换为 QImage
        int width = fz_pixmap_width(m_context, pixmap);
        int height = fz_pixmap_height(m_context, pixmap);
        int stride = fz_pixmap_stride(m_context, pixmap);
        unsigned char* samples = fz_pixmap_samples(m_context, pixmap);

        result = QImage(width, height, QImage::Format_RGB888);
        for (int y = 0; y < height; ++y) {
            unsigned char* src = samples + y * stride;
            unsigned char* dest = result.scanLine(y);
            memcpy(dest, src, width * 3);
        }

        // 清理
        fz_close_device(m_context, device);
        fz_drop_device(m_context, device);
        fz_drop_pixmap(m_context, pixmap);
        fz_drop_page(m_context, page);
    }
    fz_catch(m_context) {
        QString err = QString("Failed to render page %1: %2")
        .arg(pageIndex)
            .arg(fz_caught_message(m_context));
        setLastError(err);
        qWarning() << "ThreadSafeRenderer:" << err;
        result = QImage();
    }

    return result;
}

QSizeF ThreadSafeRenderer::getPageSize(int pageIndex)
{
    QMutexLocker locker(&m_mutex);

    if (!m_document || !m_context) {
        return QSizeF();
    }

    if (pageIndex < 0 || pageIndex >= m_pageCount) {
        return QSizeF();
    }

    QSizeF size;

    fz_try(m_context) {
        fz_page* page = fz_load_page(m_context, m_document, pageIndex);
        fz_rect bounds = fz_bound_page(m_context, page);
        size.setWidth(bounds.x1 - bounds.x0);
        size.setHeight(bounds.y1 - bounds.y0);
        fz_drop_page(m_context, page);
    }
    fz_catch(m_context) {
        QString err = QString("Failed to get page size for %1: %2")
        .arg(pageIndex)
            .arg(fz_caught_message(m_context));
        setLastError(err);
        qWarning() << "ThreadSafeRenderer:" << err;
        size = QSizeF();
    }

    return size;
}

QString ThreadSafeRenderer::getLastError() const
{
    return m_lastError;
}

void ThreadSafeRenderer::setLastError(const QString& error)
{
    m_lastError = error;
}
