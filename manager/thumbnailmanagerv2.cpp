#include "thumbnailmanagerv2.h"
#include "thumbnailcache.h"
#include "mupdfrenderer.h"
#include <QDebug>
#include <QElapsedTimer>

ThumbnailManagerV2::ThumbnailManagerV2(MuPDFRenderer* renderer, QObject* parent)
    : QObject(parent)
    , m_renderer(renderer)
    , m_cache(std::make_unique<ThumbnailCache>())
    , m_threadPool(std::make_unique<QThreadPool>())
    , m_thumbnailWidth(120)
    , m_rotation(0)
    , m_currentBatchIndex(0)
    , m_isLoadingInProgress(false)
{
    int threadCount = qMin(4, QThreadPool::globalInstance()->maxThreadCount() / 2);
    m_threadPool->setMaxThreadCount(threadCount);
    m_threadPool->setExpiryTimeout(30000);

    m_batchTimer = new QTimer(this);
    m_batchTimer->setSingleShot(true);
    m_batchTimer->setInterval(200);
    connect(m_batchTimer, &QTimer::timeout, this, &ThumbnailManagerV2::processNextBatch);

    qInfo() << "ThumbnailManagerV2: Initialized with"
            << m_threadPool->maxThreadCount() << "threads (simplified mode)";
}

ThumbnailManagerV2::~ThumbnailManagerV2()
{
    clear();
}

// ========== 配置 ==========

void ThumbnailManagerV2::setThumbnailWidth(int width)
{
    if (width >= 80 && width <= 400) {
        m_thumbnailWidth = width;
    }
}

void ThumbnailManagerV2::setRotation(int rotation)
{
    m_rotation = rotation;
}

// ========== 获取缩略图 ==========

QImage ThumbnailManagerV2::getThumbnail(int pageIndex) const
{
    return m_cache->get(pageIndex);
}

bool ThumbnailManagerV2::hasThumbnail(int pageIndex) const
{
    return m_cache->has(pageIndex);
}

int ThumbnailManagerV2::cachedCount() const
{
    return m_cache ? m_cache->count() : 0;
}

// ========== 加载控制 ==========

void ThumbnailManagerV2::startLoading(const QSet<int>& initialVisible)
{
    if (!m_renderer || !m_renderer->isDocumentLoaded()) {
        qWarning() << "ThumbnailManagerV2: No document loaded";
        return;
    }

    int pageCount = m_renderer->pageCount();
    m_strategy.reset(StrategyFactory::createStrategy(pageCount, this));

    QString strategyName;
    switch (m_strategy->type()) {
    case LoadStrategyType::SMALL_DOC:
        strategyName = "Small Document (Full Sync)";
        break;
    case LoadStrategyType::MEDIUM_DOC:
        strategyName = "Medium Document (Visible Sync + Background Async)";
        break;
    case LoadStrategyType::LARGE_DOC:
        strategyName = "Large Document (On-Demand Sync Only)";
        break;
    }

    qInfo() << "ThumbnailManagerV2: Starting load with strategy:" << strategyName;
    emit loadingStarted(pageCount, strategyName);

    QVector<int> initialPages = m_strategy->getInitialLoadPages(initialVisible);

    if (initialPages.isEmpty()) {
        return;
    }

    if (m_strategy->type() == LoadStrategyType::SMALL_DOC) {
        // 小文档：同步全量加载
        m_isLoadingInProgress = true;
        emit loadingStatusChanged(tr("Loading all thumbnails..."));
        renderPagesSync(initialPages);
        emit loadingStatusChanged(tr("All thumbnails loaded"));
        m_isLoadingInProgress = false;
        emit allCompleted();

    } else if (m_strategy->type() == LoadStrategyType::MEDIUM_DOC) {
        // 中文档：可见区同步 + 后台批次异步
        m_isLoadingInProgress = true;
        emit loadingStatusChanged(tr("加载可见区..."));
        renderPagesSync(initialPages);
        emit loadingStatusChanged(tr("后台加载中..."));
        setupBackgroundBatches();

    } else {
        // 大文档：仅同步加载初始可见区，其余按需加载
        m_isLoadingInProgress = false;  // 不标记为加载中
        emit loadingStatusChanged(tr("Loading visible thumbnails..."));
        renderPagesSync(initialPages);
        emit loadingStatusChanged(tr("Scroll to load more"));
    }
}

void ThumbnailManagerV2::syncLoadPages(const QVector<int>& pages)
{
    if (!m_renderer || pages.isEmpty()) {
        return;
    }

    // 中文档批次加载期间，忽略同步加载请求
    if (m_isLoadingInProgress) {
        qDebug() << "ThumbnailManagerV2: Ignoring sync load during batch loading";
        return;
    }

    // 过滤已缓存的页面
    QVector<int> toLoad;
    for (int pageIndex : pages) {
        if (!m_cache->has(pageIndex)) {
            toLoad.append(pageIndex);
        }
    }

    if (toLoad.isEmpty()) {
        return;
    }

    qInfo() << "ThumbnailManagerV2: Sync loading" << toLoad.size()
            << "pages (strategy:"
            << (m_strategy ? static_cast<int>(m_strategy->type()) : -1) << ")";

    // 同步渲染
    renderPagesSync(toLoad);
}

void ThumbnailManagerV2::handleSlowScroll(const QSet<int>& visiblePages)
{
    if (!m_renderer || visiblePages.isEmpty()) {
        return;
    }

    // 仅大文档且未在批次加载中时响应慢速滚动
    if (!m_strategy || m_strategy->type() != LoadStrategyType::LARGE_DOC) {
        return;
    }

    if (m_isLoadingInProgress) {
        return;
    }

    // 过滤未缓存的页面
    QVector<int> toLoad;
    for (int pageIndex : visiblePages) {
        if (!m_cache->has(pageIndex)) {
            toLoad.append(pageIndex);
        }
    }

    if (toLoad.isEmpty()) {
        return;
    }

    qDebug() << "ThumbnailManagerV2: Slow scroll detected, loading"
             << toLoad.size() << "visible pages";

    // 同步加载可见区域
    renderPagesSync(toLoad);
}

void ThumbnailManagerV2::cancelAllTasks()
{
    QMutexLocker locker(&m_taskMutex);

    for (ThumbnailBatchTask* task : m_activeTasks) {
        if (task) {
            task->abort();
        }
    }

    m_activeTasks.clear();
    m_batchTimer->stop();
    m_currentBatchIndex = 0;
}

void ThumbnailManagerV2::waitForCompletion()
{
    m_threadPool->waitForDone();
}

void ThumbnailManagerV2::clear()
{
    cancelAllTasks();
    waitForCompletion();

    if (m_cache) {
        m_cache->clear();
    }

    m_backgroundBatches.clear();
    m_currentBatchIndex = 0;
    m_isLoadingInProgress = false;
}

QString ThumbnailManagerV2::getStatistics() const
{
    return m_cache ? m_cache->getStatistics() : QString();
}

bool ThumbnailManagerV2::shouldRespondToScroll() const
{
    // 大文档在批次加载期间不响应滚动
    // 注意：大文档的m_isLoadingInProgress始终为false，所以会响应
    return !m_isLoadingInProgress;
}

// ========== 私有方法 ==========

void ThumbnailManagerV2::renderPagesSync(const QVector<int>& pages)
{
    if (!m_renderer || pages.isEmpty()) {
        return;
    }

    QElapsedTimer timer;
    timer.start();

    int rendered = 0;
    int total = pages.size();

    for (int pageIndex : pages) {
        if (m_cache->has(pageIndex)) {
            continue;
        }

        QSizeF pageSize = m_renderer->pageSize(pageIndex);
        if (pageSize.isEmpty()) {
            continue;
        }

        double zoom = m_thumbnailWidth / pageSize.width();
        MuPDFRenderer::RenderResult result = m_renderer->renderPage(
            pageIndex, zoom, m_rotation);

        if (result.success && !result.image.isNull()) {
            m_cache->set(pageIndex, result.image);
            emit thumbnailLoaded(pageIndex, result.image);
            rendered++;

            // 每渲染10页报告一次进度
            if (rendered % 10 == 0 || rendered == total) {
                emit loadProgress(rendered, total);
            }
        }
    }

    qint64 elapsed = timer.elapsed();
    qInfo() << "ThumbnailManagerV2: Sync rendered" << rendered
            << "pages in" << elapsed << "ms"
            << "(" << (rendered > 0 ? elapsed / rendered : 0) << "ms/page)";
}

void ThumbnailManagerV2::renderPagesAsync(const QVector<int>& pages, RenderPriority priority)
{
    if (!m_renderer || pages.isEmpty()) {
        return;
    }

    // 过滤已缓存的页面
    QVector<int> toRender;
    for (int pageIndex : pages) {
        if (!m_cache->has(pageIndex)) {
            toRender.append(pageIndex);
        }
    }

    if (toRender.isEmpty()) {
        return;
    }

    qDebug() << "ThumbnailManagerV2: Async rendering" << toRender.size()
             << "pages (priority:" << static_cast<int>(priority) << ")";

    auto* task = new ThumbnailBatchTask(
        m_renderer->documentPath(),
        m_cache.get(),
        this,
        toRender,
        priority,
        m_thumbnailWidth,
        m_rotation
        );

    trackTask(task);
    m_threadPool->start(task, static_cast<int>(priority));
}

void ThumbnailManagerV2::setupBackgroundBatches()
{
    if (!m_strategy) {
        return;
    }

    m_backgroundBatches = m_strategy->getBackgroundBatches();
    m_currentBatchIndex = 0;

    if (!m_backgroundBatches.isEmpty()) {
        qInfo() << "ThumbnailManagerV2: Setup" << m_backgroundBatches.size()
        << "background batches for medium document";

        // 延迟启动第一批
        QTimer::singleShot(500, this, &ThumbnailManagerV2::processNextBatch);
    }
}

void ThumbnailManagerV2::processNextBatch()
{
    if (m_currentBatchIndex >= m_backgroundBatches.size()) {
        qInfo() << "ThumbnailManagerV2: All background batches completed";

        m_isLoadingInProgress = false;  // 标记批次加载完成

        emit loadingStatusChanged(tr("All thumbnails loaded"));
        emit allCompleted();
        return;
    }

    const QVector<int>& batch = m_backgroundBatches[m_currentBatchIndex];

    qDebug() << "ThumbnailManagerV2: Processing batch"
             << (m_currentBatchIndex + 1) << "/" << m_backgroundBatches.size()
             << "(" << batch.size() << "pages)";

    emit loadingStatusChanged(tr("加载中..."));

    // 中文档后台批次使用异步渲染
    renderPagesAsync(batch, RenderPriority::LOW);

    emit batchCompleted(m_currentBatchIndex + 1, m_backgroundBatches.size());

    m_currentBatchIndex++;

    if (m_currentBatchIndex < m_backgroundBatches.size()) {
        m_batchTimer->start();
    } else {
        m_isLoadingInProgress = false;  // 标记批次加载完成
        emit allCompleted();
    }
}

void ThumbnailManagerV2::trackTask(ThumbnailBatchTask* task)
{
    QMutexLocker locker(&m_taskMutex);
    m_activeTasks.append(task);
}
