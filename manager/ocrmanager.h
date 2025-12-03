#ifndef OCRMANAGER_H
#define OCRMANAGER_H

#include <QObject>
#include <QImage>
#include <QPoint>
#include <QTimer>
#include <QRect>
#include <memory>
#include "ocrengine.h"

/**
 * @brief OCR管理器 - 全局单例
 *
 * 职责：
 * 1. 管理全局唯一的OCREngine
 * 2. 处理防抖逻辑
 * 3. 异步OCR识别
 *
 * 特点：
 * - 单例模式，整个应用共享一个实例
 * - 与PDF文档无关，可用于任何图像识别
 */
class OCRManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static OCRManager& instance();

    /**
     * @brief 初始化OCR引擎（异步）
     * @param modelDir 模型目录
     * @return 是否开始初始化
     */
    bool initialize(const QString& modelDir);

    /**
     * @brief 检查是否已就绪
     */
    bool isReady() const;

    /**
     * @brief 获取引擎状态
     */
    OCREngineState engineState() const;

    /**
     * @brief 请求OCR识别（带防抖）
     * @param image 待识别的图像区域
     * @param regionRect 区域在屏幕上的位置（用于定位浮层）
     */
    void requestOCR(const QImage& image, const QRect& regionRect);

    /**
     * @brief 取消待处理的OCR
     */
    void cancelPending();

    /**
     * @brief 设置防抖延迟（毫秒）
     */
    void setDebounceDelay(int delay);

    /**
     * @brief 获取最后的错误信息
     */
    QString lastError() const;

signals:
    /**
     * @brief OCR识别完成
     * @param result 识别结果
     * @param regionRect 识别区域（用于定位浮层）
     */
    void ocrCompleted(const OCRResult& result, const QRect& regionRect);

    /**
     * @brief OCR识别失败
     */
    void ocrFailed(const QString& error);

    /**
     * @brief 引擎状态改变
     */
    void engineStateChanged(OCREngineState state);

private:
    OCRManager();
    ~OCRManager();
    OCRManager(const OCRManager&) = delete;
    OCRManager& operator=(const OCRManager&) = delete;

private slots:
    void performOCR();
    void onEngineStateChanged(OCREngineState state);

private:
    std::unique_ptr<OCREngine> m_engine;
    QTimer m_debounceTimer;

    struct PendingRequest {
        bool valid;
        QImage image;
        QRect regionRect;

        PendingRequest() : valid(false) {}
    } m_pending;

    int m_debounceDelay;
};

#endif // OCRMANAGER_H
