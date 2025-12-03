#include "ocrfloatingwidget.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QEvent>
#include <QMouseEvent>

OCRFloatingWidget::OCRFloatingWidget(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setupUI();
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    // 安装事件过滤器，点击外部关闭
    qApp->installEventFilter(this);
}

void OCRFloatingWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(
        "QLabel {"
        "  border: 1px solid #d0d0d0;"
        "  border-radius: 4px;"
        "  background: #fafafa;"
        "  padding: 4px;"
        "}"
        );
    m_imageLabel->setMaximumSize(300, 200);
    m_imageLabel->setScaledContents(false);
    mainLayout->addWidget(m_imageLabel);

    // 文字标签
    m_textLabel = new QLabel(this);
    m_textLabel->setWordWrap(true);
    m_textLabel->setMaximumWidth(300);
    m_textLabel->setStyleSheet(
        "QLabel {"
        "  color: #333333;"
        "  font-size: 14px;"
        "  padding: 4px;"
        "}"
        );
    mainLayout->addWidget(m_textLabel);

    // 置信度标签
    m_confidenceLabel = new QLabel(this);
    m_confidenceLabel->setStyleSheet(
        "QLabel {"
        "  color: #666666;"
        "  font-size: 11px;"
        "}"
        );
    mainLayout->addWidget(m_confidenceLabel);

    // 按钮行
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_lookupButton = new QPushButton(tr("查词"), this);
    m_lookupButton->setCursor(Qt::PointingHandCursor);
    m_lookupButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #0078d4;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #005a9e;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #004578;"
        "}"
        );
    connect(m_lookupButton, &QPushButton::clicked,
            this, [this]() {
                emit lookupRequested(m_currentText);
            });
    buttonLayout->addWidget(m_lookupButton);

    m_closeButton = new QPushButton(tr("关闭"), this);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #f3f3f3;"
        "  color: #333333;"
        "  border: 1px solid #d0d0d0;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #e0e0e0;"
        "}"
        );
    connect(m_closeButton, &QPushButton::clicked,
            this, &OCRFloatingWidget::hideFloating);
    buttonLayout->addWidget(m_closeButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void OCRFloatingWidget::showResult(const QString& text, float confidence, const QRect& regionRect)
{
    m_currentText = text;

    // 隐藏图片
    m_imageLabel->hide();

    m_textLabel->setText(text);
    m_confidenceLabel->setText(tr("置信度: %1%").arg(qRound(confidence * 100)));

    adjustSize();
    positionWidget(regionRect);
    show();
    raise();
    activateWindow();
}


void OCRFloatingWidget::showResult(const QString& text, float confidence, const QRect& regionRect, const QImage& sourceImage)
{
    m_currentText = text;

    // 显示原始识别图片
    if (!sourceImage.isNull()) {
        // 缩放图片以适应标签大小
        QPixmap pixmap = QPixmap::fromImage(sourceImage);

        // 保持宽高比缩放
        QSize labelSize = m_imageLabel->maximumSize();
        if (pixmap.width() > labelSize.width() || pixmap.height() > labelSize.height()) {
            pixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        m_imageLabel->setPixmap(pixmap);
        m_imageLabel->show();
    } else {
        m_imageLabel->hide();
    }

    m_textLabel->setText(text);
    m_confidenceLabel->setText(tr("置信度: %1%").arg(qRound(confidence * 100)));

    adjustSize();
    positionWidget(regionRect);
    show();
    raise();
    activateWindow();
}

void OCRFloatingWidget::hideFloating()
{
    hide();
    m_currentText.clear();
    m_imageLabel->clear();
    m_imageLabel->hide();
}

void OCRFloatingWidget::positionWidget(const QRect& regionRect)
{
    // 默认显示在识别区域下方
    int x = regionRect.x();
    int y = regionRect.bottom() + 10;

    // 确保不超出屏幕
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();

        if (x + width() > screenGeometry.right()) {
            x = screenGeometry.right() - width();
        }
        if (x < screenGeometry.left()) {
            x = screenGeometry.left();
        }

        // 如果下方空间不足，显示在上方
        if (y + height() > screenGeometry.bottom()) {
            y = regionRect.top() - height() - 10;
        }

        if (y < screenGeometry.top()) {
            y = screenGeometry.top();
        }
    }

    move(x, y);
}

void OCRFloatingWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制圆角背景
    QPainterPath path;
    path.addRoundedRect(rect(), 8, 8);

    // 阴影效果
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawPath(path.translated(2, 2));

    // 背景
    painter.setBrush(QColor(255, 255, 255, 250));
    painter.drawPath(path);

    // 边框
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    QWidget::paintEvent(event);
}

bool OCRFloatingWidget::eventFilter(QObject* obj, QEvent* event)
{
    // 点击浮层外部时关闭
    if (event->type() == QEvent::MouseButtonPress && isVisible()) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!geometry().contains(mouseEvent->globalPosition().toPoint())) {
            hideFloating();
        }
    }

    return QWidget::eventFilter(obj, event);
}
