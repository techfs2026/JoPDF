#include "threadsaferenderer.h"
#include <QDebug>
#include <cstring>
#include <QThread>

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

PageTextData ThreadSafeRenderer::extractPageText(int pageIndex)
{
    QMutexLocker locker(&m_mutex);

    PageTextData pageData;

    if (!m_document || !m_context) {
        setLastError("No document loaded");
        qWarning() << "ThreadSafeRenderer::extractPageText: No document loaded";
        return pageData;
    }

    if (pageIndex < 0 || pageIndex >= m_pageCount) {
        QString err = QString("Invalid page index: %1 (valid range: 0-%2)")
        .arg(pageIndex).arg(m_pageCount - 1);
        setLastError(err);
        qWarning() << "ThreadSafeRenderer::extractPageText:" << err;
        return pageData;
    }

    pageData.pageIndex = pageIndex;
    fz_stext_page* stext = nullptr;
    fz_page* page = nullptr;

    fz_try(m_context) {
        // 加载页面
        page = fz_load_page(m_context, m_document, pageIndex);
        if (!page) {
            setLastError(QString("Failed to load page %1").arg(pageIndex));
            qWarning() << "ThreadSafeRenderer::extractPageText: Page is null after load";
            return pageData;
        }

        fz_rect bound = fz_bound_page(m_context, page);

        qDebug() << "ThreadSafeRenderer::extractPageText: Page" << pageIndex
                 << "bounds:" << bound.x0 << bound.y0 << bound.x1 << bound.y1;

        // 创建结构化文本页面
        stext = fz_new_stext_page(m_context, bound);
        if (!stext) {
            setLastError(QString("Failed to create stext page for page %1").arg(pageIndex));
            fz_drop_page(m_context, page);
            return pageData;
        }

        // 提取文本
        fz_stext_options options;
        memset(&options, 0, sizeof(options));
        fz_device* dev = fz_new_stext_device(m_context, stext, &options);
        fz_run_page(m_context, page, dev, fz_identity, nullptr);
        fz_close_device(m_context, dev);
        fz_drop_device(m_context, dev);

        // 遍历结构化文本，转换到 Qt 类型
        int blockCount = 0;
        int lineCount = 0;
        int charCount = 0;

        for (fz_stext_block* block = stext->first_block; block; block = block->next) {
            if (block->type != FZ_STEXT_BLOCK_TEXT) continue;
            blockCount++;

            TextBlock tb;
            tb.bbox = QRectF(block->bbox.x0, block->bbox.y0,
                             block->bbox.x1 - block->bbox.x0,
                             block->bbox.y1 - block->bbox.y0);

            for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
                lineCount++;
                TextLine tl;
                tl.bbox = QRectF(line->bbox.x0, line->bbox.y0,
                                 line->bbox.x1 - line->bbox.x0,
                                 line->bbox.y1 - line->bbox.y0);

                for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                    charCount++;
                    TextChar tc;
                    tc.character = QChar(ch->c);
                    fz_quad q = ch->quad;
                    qreal minX = qMin(qMin(q.ul.x, q.ur.x), qMin(q.ll.x, q.lr.x));
                    qreal maxX = qMax(qMax(q.ul.x, q.ur.x), qMax(q.ll.x, q.lr.x));
                    qreal minY = qMin(qMin(q.ul.y, q.ur.y), qMin(q.ll.y, q.lr.y));
                    qreal maxY = qMax(qMax(q.ul.y, q.ur.y), qMax(q.ll.y, q.lr.y));
                    tc.bbox = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
                    tl.chars.append(tc);
                    pageData.fullText.append(tc.character);
                }
                tb.lines.append(tl);
                pageData.fullText.append('\n');
            }
            pageData.blocks.append(tb);
            pageData.fullText.append("\n\n");
        }

        qDebug() << "ThreadSafeRenderer::extractPageText: Page" << pageIndex
                 << "extracted - blocks:" << blockCount
                 << "lines:" << lineCount
                 << "chars:" << charCount
                 << "fullText length:" << pageData.fullText.length();

        // 清理资源
        if (stext) {
            fz_drop_stext_page(m_context, stext);
            stext = nullptr;
        }
        fz_drop_page(m_context, page);
        page = nullptr;

        // 清除错误（成功）
        setLastError("");
    }
    fz_catch(m_context) {
        QString err = QString("Failed to extract text from page %1: %2")
        .arg(pageIndex)
            .arg(fz_caught_message(m_context));
        setLastError(err);
        qWarning() << "ThreadSafeRenderer::extractPageText:" << err;

        if (stext) {
            fz_drop_stext_page(m_context, stext);
        }
        if (page) {
            fz_drop_page(m_context, page);
        }

        // 返回空数据
        pageData = PageTextData();
        pageData.pageIndex = pageIndex;
    }

    return pageData;
}

QString ThreadSafeRenderer::getLastError() const
{
    return m_lastError;
}

void ThreadSafeRenderer::setLastError(const QString& error)
{
    m_lastError = error;
}
