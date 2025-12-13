#ifndef PAPEREFFECTENHANCER_SIMPLE_H
#define PAPEREFFECTENHANCER_SIMPLE_H

#include <opencv2/opencv.hpp>
#include <QImage>
#include <QMutex>

struct SimpleOptions {
    bool enabled = true;

    // 核心参数：背景纸张颜色 (米黄色)
    cv::Vec3b paperColor = cv::Vec3b(220, 248, 255); // BGR: 米黄色 #FFF8DC

    // 着色强度 (0.0 = 保持原样, 1.0 = 完全替换为纸张色)
    double colorIntensity = 0.7;

    // 文字/背景分离阈值 (0-255, 越高越多区域被认为是背景)
    int threshold = 200;

    // 边缘羽化半径 (避免文字边缘生硬, 0=不羽化)
    int featherRadius = 2;

    // 预设颜色选项
    enum PaperPreset {
        WARM_WHITE,      // 暖白色 #FFF8DC
        CREAM,           // 奶油色 #FAEBD7
        LIGHT_YELLOW,    // 浅黄色 #FFFACD
        SEPIA,           // 复古棕 #F4ECD8
        CUSTOM           // 自定义
    };

    void setPaperPreset(PaperPreset preset) {
        switch(preset) {
        case WARM_WHITE:
            paperColor = cv::Vec3b(220, 248, 255); // #FFF8DC
            break;
        case CREAM:
            paperColor = cv::Vec3b(215, 235, 250); // #FAEBD7
            break;
        case LIGHT_YELLOW:
            paperColor = cv::Vec3b(205, 250, 255); // #FFFACD
            break;
        case SEPIA:
            paperColor = cv::Vec3b(216, 236, 244); // #F4ECD8
            break;
        case CUSTOM:
            // 保持当前自定义颜色
            break;
        }
    }
};

class PaperEffectEnhancer
{
public:
    explicit PaperEffectEnhancer(const SimpleOptions& opt = SimpleOptions());
    ~PaperEffectEnhancer();

    // 主要处理函数
    QImage enhance(const QImage& input);

    // 设置和获取参数
    void setOptions(const SimpleOptions& opt);
    SimpleOptions options() const { return m_options; }

private:
    // 格式转换
    cv::Mat qImageToCvMat(const QImage& image);
    QImage cvMatToQImage(const cv::Mat& mat);

    // 核心处理函数
    cv::Mat createTextMask(const cv::Mat& img);
    void applyPaperBackground(cv::Mat& img, const cv::Mat& textMask);
    void featherMask(cv::Mat& mask, int radius);

    SimpleOptions m_options;
    QMutex m_mutex;
};

#endif // PAPEREFFECTENHANCER_SIMPLE_H
