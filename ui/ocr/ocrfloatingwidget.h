#ifndef OCRFLOATINGWIDGET_H
#define OCRFLOATINGWIDGET_H

#include <QWidget>

class QLabel;
class QPushButton;

class OCRFloatingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OCRFloatingWidget(QWidget* parent = nullptr);

    void showResult(const QString& text, float confidence, const QRect& regionRect);

    // ===== 添加：同时显示识别图片 =====
    void showResult(const QString& text, float confidence, const QRect& regionRect, const QImage& sourceImage);

    void hideFloating();

signals:
    void lookupRequested(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUI();
    void positionWidget(const QRect& regionRect);

private:
    QLabel* m_textLabel;
    QLabel* m_confidenceLabel;
    QLabel* m_imageLabel;
    QPushButton* m_lookupButton;
    QPushButton* m_closeButton;

    QString m_currentText;
};

#endif // OCRFLOATINGWIDGET_H
