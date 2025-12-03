#include "ocrstatusindicator.h"
#include <QPainter>
#include <QMouseEvent>

OCRStatusIndicator::OCRStatusIndicator(QWidget* parent)
    : QWidget(parent)
    , m_state(OCREngineState::Uninitialized)
{
    setFixedSize(80, 24);
    setCursor(Qt::PointingHandCursor);
}

void OCRStatusIndicator::setState(OCREngineState state)
{
    if (m_state != state) {
        m_state = state;
        update();

        // 更新工具提示
        QString tooltip;
        switch (state) {
        case OCREngineState::Uninitialized:
            tooltip = tr("OCR未初始化");
            break;
        case OCREngineState::Loading:
            tooltip = tr("OCR加载中...");
            break;
        case OCREngineState::Ready:
            tooltip = tr("OCR已就绪");
            break;
        case OCREngineState::Error:
            tooltip = tr("OCR初始化失败");
            break;
        }
        setToolTip(tooltip);
    }
}

QSize OCRStatusIndicator::sizeHint() const
{
    return QSize(80, 24);
}

void OCRStatusIndicator::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    painter.fillRect(rect(), QColor(240, 240, 240));

    // 绘制指示灯
    int indicatorSize = 12;
    int indicatorX = 10;
    int indicatorY = (height() - indicatorSize) / 2;

    QColor lightColor;
    switch (m_state) {
    case OCREngineState::Uninitialized:
        lightColor = QColor(160, 160, 160);  // 灰色
        break;
    case OCREngineState::Loading:
        lightColor = QColor(255, 193, 7);    // 黄色
        break;
    case OCREngineState::Ready:
        lightColor = QColor(76, 175, 80);    // 绿色
        break;
    case OCREngineState::Error:
        lightColor = QColor(244, 67, 54);    // 红色
        break;
    }

    // 外圈阴影
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 30));
    painter.drawEllipse(indicatorX + 1, indicatorY + 1, indicatorSize, indicatorSize);

    // 指示灯
    QRadialGradient gradient(indicatorX + indicatorSize/2, indicatorY + indicatorSize/2,
                             indicatorSize/2);
    gradient.setColorAt(0, lightColor.lighter(120));
    gradient.setColorAt(1, lightColor);
    painter.setBrush(gradient);
    painter.drawEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);

    // 高光
    painter.setBrush(QColor(255, 255, 255, 100));
    painter.drawEllipse(indicatorX + 2, indicatorY + 2, indicatorSize/3, indicatorSize/3);

    // 绘制文字
    QString statusText;
    switch (m_state) {
    case OCREngineState::Uninitialized:
        statusText = tr("未启用");
        break;
    case OCREngineState::Loading:
        statusText = tr("加载中");
        break;
    case OCREngineState::Ready:
        statusText = tr("就绪");
        break;
    case OCREngineState::Error:
        statusText = tr("错误");
        break;
    }

    painter.setPen(QColor(60, 60, 60));
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    painter.drawText(indicatorX + indicatorSize + 6, indicatorY,
                     width() - indicatorX - indicatorSize - 6, indicatorSize,
                     Qt::AlignVCenter | Qt::AlignLeft, statusText);
}

void OCRStatusIndicator::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    emit doubleClicked();
}
