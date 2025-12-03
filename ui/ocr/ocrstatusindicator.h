#ifndef OCRSTATUSINDICATOR_H
#define OCRSTATUSINDICATOR_H

#include <QWidget>
#include "ocrengine.h"

/**
 * @brief OCR状态指示器 - 红黄绿灯
 *
 * 显示在状态栏，指示OCR功能状态
 */
class OCRStatusIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit OCRStatusIndicator(QWidget* parent = nullptr);

    /**
     * @brief 设置状态
     */
    void setState(OCREngineState state);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

signals:
    /**
     * @brief 双击指示器
     */
    void doubleClicked();

private:
    OCREngineState m_state;
};

#endif // OCRSTATUSINDICATOR_H
