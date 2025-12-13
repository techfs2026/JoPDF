#include "ocrstatusindicator.h"
#include <QPainter>
#include <QMouseEvent>

OCRStatusIndicator::OCRStatusIndicator(QWidget* parent)
    : QWidget(parent)
    , m_state(OCREngineState::Uninitialized)
    , m_engineRunning(false)
    , m_hovered(false)
    , m_pressed(false)
{
    setMinimumSize(90, 24);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    setToolTip(getTooltipText());
}

void OCRStatusIndicator::setState(OCREngineState state)
{
    if (m_state != state) {
        m_state = state;
        setToolTip(getTooltipText());
        updateGeometry();
        update();
    }
}

void OCRStatusIndicator::setEngineRunning(bool running)
{
    if (m_engineRunning != running) {
        m_engineRunning = running;
        setToolTip(getTooltipText());
        updateGeometry();
        update();
    }
}

QString OCRStatusIndicator::getStatusText() const
{
    if (!m_engineRunning) {
        return tr("启动OCR");
    }

    switch (m_state) {
    case OCREngineState::Uninitialized:
        return tr("未初始化");
    case OCREngineState::Loading:
        return tr("加载中...");
    case OCREngineState::Ready:
        return tr("OCR就绪");
    case OCREngineState::Error:
        return tr("初始化失败");
    default:
        return tr("未知状态");
    }
}

QString OCRStatusIndicator::getTooltipText() const
{
    if (!m_engineRunning) {
        return tr("点击启动OCR引擎\n"
                  "启动后可在工具栏启用OCR取词功能");
    }

    switch (m_state) {
    case OCREngineState::Loading:
        return tr("OCR引擎加载中...\n"
                  "请稍候，加载完成后可启用OCR取词\n"
                  "双击停止引擎");
    case OCREngineState::Ready:
        return tr("OCR引擎已就绪 ✓\n"
                  "可在工具栏启用OCR取词功能\n"
                  "双击停止引擎");
    case OCREngineState::Error:
        return tr("OCR引擎初始化失败\n"
                  "请检查模型文件和配置\n"
                  "双击重新启动");
    case OCREngineState::Uninitialized:
        return tr("OCR引擎未初始化\n"
                  "点击启动引擎");
    default:
        return tr("OCR引擎状态未知");
    }
}

QColor OCRStatusIndicator::getLightColor() const
{
    if (!m_engineRunning) {
        return QColor(160, 160, 160);  // 灰色 - 未运行
    }

    switch (m_state) {
    case OCREngineState::Uninitialized:
        return QColor(160, 160, 160);  // 灰色
    case OCREngineState::Loading:
        return QColor(255, 193, 7);    // 黄色 - 加载中
    case OCREngineState::Ready:
        return QColor(76, 175, 80);    // 绿色 - 就绪
    case OCREngineState::Error:
        return QColor(244, 67, 54);    // 红色 - 错误
    default:
        return QColor(160, 160, 160);
    }
}

QSize OCRStatusIndicator::sizeHint() const
{
    // 根据当前文本计算最佳宽度
    QString text = getStatusText();
    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(text);

    // 指示灯 + 间距 + 文字 + 边距
    // 8(左边距) + 14(指示灯) + 6(间距) + textWidth + 10(右边距)
    int totalWidth = 8 + 14 + 6 + textWidth + 10;

    // 确保最小宽度
    totalWidth = qMax(totalWidth, 90);

    return QSize(totalWidth, 24);
}

void OCRStatusIndicator::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景 - 根据状态改变背景色
    QColor bgColor;
    if (m_pressed) {
        bgColor = QColor(220, 220, 220);
    } else if (m_hovered) {
        if (m_engineRunning && m_state == OCREngineState::Ready) {
            bgColor = QColor(232, 245, 233);  // 浅绿色
        } else {
            bgColor = QColor(235, 235, 235);
        }
    } else {
        if (m_engineRunning && m_state == OCREngineState::Ready) {
            bgColor = QColor(240, 248, 240);  // 极浅绿色
        } else {
            bgColor = QColor(245, 245, 245);
        }
    }

    // 绘制圆角背景
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

    // 绘制边框
    if (m_hovered || m_pressed) {
        painter.setPen(QPen(QColor(200, 200, 200), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
    }

    // 绘制指示灯
    int indicatorSize = 14;
    int indicatorX = 8;
    int indicatorY = (height() - indicatorSize) / 2;

    QColor lightColor = getLightColor();

    // 外圈阴影
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 30));
    painter.drawEllipse(indicatorX + 1, indicatorY + 1, indicatorSize, indicatorSize);

    // 指示灯 - 渐变效果
    QRadialGradient gradient(indicatorX + indicatorSize/2, indicatorY + indicatorSize/2,
                             indicatorSize/2);
    gradient.setColorAt(0, lightColor.lighter(130));
    gradient.setColorAt(0.7, lightColor);
    gradient.setColorAt(1, lightColor.darker(110));
    painter.setBrush(gradient);
    painter.drawEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);

    // 高光效果
    painter.setBrush(QColor(255, 255, 255, 120));
    painter.drawEllipse(indicatorX + 3, indicatorY + 3, indicatorSize/3, indicatorSize/3);

    // 加载中动画效果（可选：添加旋转动画）
    if (m_engineRunning && m_state == OCREngineState::Loading) {
        painter.setPen(QPen(lightColor.darker(120), 2));
        painter.drawArc(indicatorX + 1, indicatorY + 1,
                        indicatorSize - 2, indicatorSize - 2,
                        0, 270 * 16);
    }

    // 绘制文字
    QString statusText = getStatusText();

    QColor textColor;
    if (!m_engineRunning) {
        textColor = QColor(100, 100, 100);  // 灰色文字
    } else if (m_state == OCREngineState::Ready) {
        textColor = QColor(46, 125, 50);    // 深绿色文字
    } else if (m_state == OCREngineState::Error) {
        textColor = QColor(198, 40, 40);    // 深红色文字
    } else {
        textColor = QColor(70, 70, 70);     // 深灰色文字
    }

    painter.setPen(textColor);
    QFont font = painter.font();
    font.setPointSize(9);

    // 就绪状态使用粗体
    if (m_engineRunning && m_state == OCREngineState::Ready) {
        font.setBold(true);
    }

    painter.setFont(font);

    QRect textRect(indicatorX + indicatorSize + 6, 0,
                   width() - indicatorX - indicatorSize - 10, height());
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, statusText);
}

void OCRStatusIndicator::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void OCRStatusIndicator::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_pressed) {
        m_pressed = false;

        if (rect().contains(event->pos())) {
            if (!m_engineRunning || m_state == OCREngineState::Error) {
                emit engineStartRequested();
            }
            emit clicked();
        }

        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void OCRStatusIndicator::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_engineRunning) {
            emit engineStopRequested();
        }

        emit doubleClicked();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void OCRStatusIndicator::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    m_hovered = true;
    update();
}

void OCRStatusIndicator::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hovered = false;
    m_pressed = false;
    update();
}
