#include "ocrmanager.h"
#include <QDebug>
#include <QtConcurrent>

OCRManager::OCRManager()
    : m_debounceDelay(300)
{
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout,
            this, &OCRManager::performOCR);
}

OCRManager::~OCRManager()
{
    cancelPending();
}

OCRManager& OCRManager::instance()
{
    static OCRManager instance;
    return instance;
}

bool OCRManager::initialize(const QString& modelDir)
{
    if (m_engine) {
        qWarning() << "OCRManager: Already initialized";
        return false;
    }

    qInfo() << "OCRManager: Initializing with model dir:" << modelDir;

    m_engine = std::make_unique<OCREngine>();

    // 连接引擎信号
    connect(m_engine.get(), &OCREngine::stateChanged,
            this, &OCRManager::onEngineStateChanged);

    connect(m_engine.get(), &OCREngine::initialized,
            this, [](bool success, const QString& error) {
                if (success) {
                    qInfo() << "OCRManager: Engine initialized successfully";
                } else {
                    qWarning() << "OCRManager: Engine initialization failed:" << error;
                }
            });

    return m_engine->initializeSync(modelDir);
}

bool OCRManager::isReady() const
{
    return m_engine && m_engine->state() == OCREngineState::Ready;
}

OCREngineState OCRManager::engineState() const
{
    return m_engine ? m_engine->state() : OCREngineState::Uninitialized;
}

void OCRManager::requestOCR(const QImage& image, const QRect& regionRect)
{
    if (!m_engine) {
        emit ocrFailed(tr("OCR引擎未初始化"));
        return;
    }

    if (m_engine->state() != OCREngineState::Ready) {
        emit ocrFailed(tr("OCR引擎未就绪"));
        return;
    }

    if (image.isNull()) {
        emit ocrFailed(tr("图像无效"));
        return;
    }

    // 取消之前的请求
    m_debounceTimer.stop();

    // 记录新请求
    m_pending.valid = true;
    m_pending.image = image;
    m_pending.regionRect = regionRect;

    // 启动防抖定时器
    m_debounceTimer.start(m_debounceDelay);
}

void OCRManager::cancelPending()
{
    m_debounceTimer.stop();
    m_pending.valid = false;
}

void OCRManager::performOCR()
{
    if (!m_pending.valid) {
        return;
    }

    // 取出请求
    QImage image = m_pending.image;
    QRect regionRect = m_pending.regionRect;
    m_pending.valid = false;

    // 在后台线程执行OCR
    QFuture<OCRResult> future = QtConcurrent::run([this, image]() {
        return m_engine->recognize(image);
    });

    // 使用QFutureWatcher监听结果
    QFutureWatcher<OCRResult>* watcher = new QFutureWatcher<OCRResult>(this);

    connect(watcher, &QFutureWatcher<OCRResult>::finished,
            this, [this, watcher, regionRect]() {
                OCRResult result = watcher->result();

                if (result.success) {
                    emit ocrCompleted(result, regionRect);
                } else {
                    emit ocrFailed(result.error);
                }

                watcher->deleteLater();
            });

    watcher->setFuture(future);
}

void OCRManager::onEngineStateChanged(OCREngineState state)
{
    emit engineStateChanged(state);
}

void OCRManager::setDebounceDelay(int delay)
{
    if (delay >= 0 && delay <= 2000) {
        m_debounceDelay = delay;
    }
}

QString OCRManager::lastError() const
{
    return m_engine ? m_engine->lastError() : tr("引擎未初始化");
}
