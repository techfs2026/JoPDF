#include "papereffectenhancer.h"
#include <QMutexLocker>

PaperEffectEnhancer::PaperEffectEnhancer(const SimpleOptions& opt)
    : m_options(opt)
{
}

PaperEffectEnhancer::~PaperEffectEnhancer()
{
}

QImage PaperEffectEnhancer::enhance(const QImage& input)
{
    QMutexLocker locker(&m_mutex);

    if (!m_options.enabled || input.isNull()) {
        return input;
    }

    // 转换为 OpenCV Mat
    cv::Mat img = qImageToCvMat(input);
    if (img.empty()) {
        return input;
    }

    // 1. 创建文字遮罩 (黑色=文字, 白色=背景)
    cv::Mat textMask = createTextMask(img);

    // 2. 应用纸张背景色
    applyPaperBackground(img, textMask);

    // 转换回 QImage
    return cvMatToQImage(img);
}

void PaperEffectEnhancer::setOptions(const SimpleOptions& opt)
{
    QMutexLocker locker(&m_mutex);
    m_options = opt;
}

cv::Mat PaperEffectEnhancer::qImageToCvMat(const QImage& image)
{
    cv::Mat mat;
    switch (image.format()) {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    {
        mat = cv::Mat(image.height(), image.width(), CV_8UC4,
                      const_cast<uchar*>(image.bits()),
                      static_cast<size_t>(image.bytesPerLine()));
        cv::Mat result;
        cv::cvtColor(mat, result, cv::COLOR_RGBA2BGR);
        return result.clone();
    }
    case QImage::Format_RGB888:
    {
        mat = cv::Mat(image.height(), image.width(), CV_8UC3,
                      const_cast<uchar*>(image.bits()),
                      static_cast<size_t>(image.bytesPerLine()));
        cv::Mat result;
        cv::cvtColor(mat, result, cv::COLOR_RGB2BGR);
        return result.clone();
    }
    case QImage::Format_Grayscale8:
    {
        mat = cv::Mat(image.height(), image.width(), CV_8UC1,
                      const_cast<uchar*>(image.bits()),
                      static_cast<size_t>(image.bytesPerLine()));
        return mat.clone();
    }
    default:
    {
        QImage convertedImage = image.convertToFormat(QImage::Format_RGB888);
        return qImageToCvMat(convertedImage);
    }
    }
}

QImage PaperEffectEnhancer::cvMatToQImage(const cv::Mat& mat)
{
    switch (mat.type()) {
    case CV_8UC1:
    {
        QImage image(mat.data, mat.cols, mat.rows,
                     static_cast<int>(mat.step), QImage::Format_Grayscale8);
        return image.copy();
    }
    case CV_8UC3:
    {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows,
                     static_cast<int>(rgb.step), QImage::Format_RGB888);
        return image.copy();
    }
    case CV_8UC4:
    {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        QImage image(rgba.data, rgba.cols, rgba.rows,
                     static_cast<int>(rgba.step), QImage::Format_ARGB32);
        return image.copy();
    }
    default:
        return QImage();
    }
}

cv::Mat PaperEffectEnhancer::createTextMask(const cv::Mat& img)
{
    cv::Mat gray;

    // 转换为灰度图
    if (img.channels() == 3) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = img.clone();
    }

    // 创建遮罩：亮度低于阈值的是文字(0), 高于阈值的是背景(255)
    cv::Mat mask;
    cv::threshold(gray, mask, m_options.threshold, 255, cv::THRESH_BINARY);

    // 轻微膨胀，避免文字边缘有白色残留
    if (m_options.featherRadius > 0) {
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                   cv::Size(3, 3));
        cv::erode(mask, mask, kernel, cv::Point(-1, -1), 1);
    }

    // 羽化边缘，使过渡更自然
    if (m_options.featherRadius > 0) {
        featherMask(mask, m_options.featherRadius);
    }

    return mask;
}

void PaperEffectEnhancer::applyPaperBackground(cv::Mat& img, const cv::Mat& textMask)
{
    // 创建纸张背景图
    cv::Mat paperBackground(img.size(), img.type(), m_options.paperColor);

    // 如果是灰度图，转换纸张颜色为灰度
    if (img.channels() == 1) {
        cv::cvtColor(paperBackground, paperBackground, cv::COLOR_BGR2GRAY);
    }

    // 根据 colorIntensity 混合原图和纸张背景
    // intensity=0: 完全保持原图
    // intensity=1: 背景完全变为纸张色
    cv::Mat blendedBackground;
    cv::addWeighted(img, 1.0 - m_options.colorIntensity,
                    paperBackground, m_options.colorIntensity,
                    0, blendedBackground);

    // 使用遮罩合成：文字区域用原图，背景区域用混合后的纸张色
    // textMask: 0=文字, 255=背景
    cv::Mat result = img.clone();

    // 归一化遮罩到 0-1 范围
    cv::Mat maskFloat;
    textMask.convertTo(maskFloat, CV_32F, 1.0/255.0);

    // 分通道混合
    if (img.channels() == 3) {
        std::vector<cv::Mat> channels(3);
        std::vector<cv::Mat> bgChannels(3);
        cv::split(img, channels);
        cv::split(blendedBackground, bgChannels);

        for (int i = 0; i < 3; i++) {
            channels[i].convertTo(channels[i], CV_32F);
            bgChannels[i].convertTo(bgChannels[i], CV_32F);

            // 文字区域(mask=0)保持原图，背景区域(mask=1)使用纸张色
            channels[i] = channels[i].mul(1.0 - maskFloat) + bgChannels[i].mul(maskFloat);
            channels[i].convertTo(channels[i], CV_8U);
        }
        cv::merge(channels, result);
    } else {
        cv::Mat imgFloat, bgFloat;
        img.convertTo(imgFloat, CV_32F);
        blendedBackground.convertTo(bgFloat, CV_32F);

        result = imgFloat.mul(1.0 - maskFloat) + bgFloat.mul(maskFloat);
        result.convertTo(result, CV_8U);
    }

    img = result;
}

void PaperEffectEnhancer::featherMask(cv::Mat& mask, int radius)
{
    if (radius <= 0) return;

    // 使用高斯模糊实现羽化效果
    int kernelSize = radius * 2 + 1;
    cv::GaussianBlur(mask, mask, cv::Size(kernelSize, kernelSize),
                     static_cast<double>(radius) / 2.0);
}
