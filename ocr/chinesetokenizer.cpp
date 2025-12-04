#include "chinesetokenizer.h"
#include <QFileInfo>
#include <QDebug>
#include <cmath>

ChineseTokenizer& ChineseTokenizer::instance()
{
    static ChineseTokenizer instance;
    return instance;
}

ChineseTokenizer::ChineseTokenizer()
    : m_initialized(false)
{
}

ChineseTokenizer::~ChineseTokenizer()
{
}

bool ChineseTokenizer::initialize(const QString& dictDir)
{
    if (m_initialized) {
        qInfo() << "ChineseTokenizer already initialized";
        return true;
    }

    m_dictDir = dictDir;

    // 检查必要的词典文件
    QString dictPath = dictDir + "/jieba.dict.utf8";
    QString hmmPath = dictDir + "/hmm_model.utf8";
    QString userDictPath = dictDir + "/user.dict.utf8";
    QString idfPath = dictDir + "/idf.utf8";
    QString stopWordsPath = dictDir + "/stop_words.utf8";

    QStringList missingFiles;
    if (!QFileInfo::exists(dictPath)) missingFiles << "jieba.dict.utf8";
    if (!QFileInfo::exists(hmmPath)) missingFiles << "hmm_model.utf8";
    if (!QFileInfo::exists(userDictPath)) missingFiles << "user.dict.utf8";
    if (!QFileInfo::exists(idfPath)) missingFiles << "idf.utf8";
    if (!QFileInfo::exists(stopWordsPath)) missingFiles << "stop_words.utf8";

    if (!missingFiles.isEmpty()) {
        m_lastError = "缺少词典文件: " + missingFiles.join(", ");
        qWarning() << m_lastError;
        return false;
    }

    try {
        // 创建 Jieba 实例
        m_jieba = std::make_unique<cppjieba::Jieba>(
            dictPath.toStdString(),
            hmmPath.toStdString(),
            userDictPath.toStdString(),
            idfPath.toStdString(),
            stopWordsPath.toStdString()
            );

        m_initialized = true;
        qInfo() << "ChineseTokenizer initialized successfully";
        return true;

    } catch (const std::exception& e) {
        m_lastError = QString("初始化失败: %1").arg(QString::fromStdString(e.what()));
        qWarning() << m_lastError;
        return false;
    }
}

QStringList ChineseTokenizer::tokenize(const QString& text)
{
    if (!m_initialized) {
        qWarning() << "ChineseTokenizer not initialized";
        return QStringList();
    }

    if (text.isEmpty()) {
        return QStringList();
    }

    try {
        std::vector<std::string> words;
        std::string textStd = text.toStdString();

        // 使用精确模式分词
        m_jieba->Cut(textStd, words, false);

        QStringList result;
        for (const auto& word : words) {
            QString qword = QString::fromStdString(word).trimmed();
            // 过滤空白和标点
            if (!qword.isEmpty() && qword != " ") {
                result << qword;
            }
        }

        return result;

    } catch (const std::exception& e) {
        qWarning() << "Tokenization error:" << e.what();
        return QStringList();
    }
}

QVector<TokenWithPosition> ChineseTokenizer::tokenizeWithPosition(
    const OCRResult& ocr)
{
    QVector<TokenWithPosition> result;
    if (!m_initialized) return result;
    if (!ocr.success) return result;
    if (ocr.texts.empty() || ocr.boxes.empty()) return result;

    size_t n = std::min(ocr.texts.size(), ocr.boxes.size());

    for (size_t i = 0; i < n; ++i) {
        const std::string& lineStd = ocr.texts[i];
        if (lineStd.empty()) continue;

        // 1. 这一行对应的 box → 行矩形
        const auto& box = ocr.boxes[i];
        if (box.size() < 4) continue;

        QRect lineRect = boundingRectFromBox(box);

        // 2. 用 jieba 对这一行分词（带 offset）
        std::vector<cppjieba::Word> words;
        m_jieba->Cut(lineStd, words, false);

        int totalLength = static_cast<int>(lineStd.length()); // 注意：offset 和 length 跟它同源即可

        for (const auto& w : words) {
            QString qword = QString::fromStdString(w.word).trimmed();
            if (qword.isEmpty()) continue;
            if (qword.length() == 1 && !qword[0].isLetterOrNumber()) {
                continue;
            }

            TokenWithPosition token;
            token.word = qword;
            token.startIndex = static_cast<int>(w.offset);
            token.endIndex   = static_cast<int>(w.offset + w.word.length());
            token.lineIndex  = static_cast<int>(i);

            token.estimatedRect = estimateWordRectInLine(
                token.startIndex,
                token.endIndex,
                totalLength,
                lineRect
                );

            result.append(token);
        }
    }

    qDebug() << "Tokenized" << result.size() << "words from OCRResult";
    return result;
}

QRect ChineseTokenizer::boundingRectFromBox(
    const std::vector<cv::Point2f>& box)
{
    float minX = box[0].x, maxX = box[0].x;
    float minY = box[0].y, maxY = box[0].y;

    for (size_t i = 1; i < box.size(); ++i) {
        minX = std::min(minX, box[i].x);
        maxX = std::max(maxX, box[i].x);
        minY = std::min(minY, box[i].y);
        maxY = std::max(maxY, box[i].y);
    }

    return QRect(
        static_cast<int>(std::floor(minX)),
        static_cast<int>(std::floor(minY)),
        static_cast<int>(std::ceil(maxX - minX)),
        static_cast<int>(std::ceil(maxY - minY))
        );
}

QRect ChineseTokenizer::estimateWordRectInLine(
    int startIndex,
    int endIndex,
    int totalLength,
    const QRect& lineRect)
{
    if (totalLength <= 0) {
        return lineRect;
    }

    double startRatio = static_cast<double>(startIndex) / totalLength;
    double endRatio   = static_cast<double>(endIndex)   / totalLength;

    int wordLeft  = lineRect.left() + static_cast<int>(startRatio * lineRect.width());
    int wordRight = lineRect.left() + static_cast<int>(endRatio   * lineRect.width());

    if (wordRight <= wordLeft) {
        wordRight = wordLeft + 1;
    }

    return QRect(
        wordLeft,
        lineRect.top(),
        wordRight - wordLeft,
        lineRect.height()
        );
}


TokenWithPosition ChineseTokenizer::findClosestToken(
    const QVector<TokenWithPosition>& tokens,
    const QPoint& mousePos)
{
    if (tokens.isEmpty()) {
        return TokenWithPosition();
    }

    double minDistance = std::numeric_limits<double>::max();
    TokenWithPosition closestToken;

    for (const auto& token : tokens) {
        double dist = distanceToRect(mousePos, token.estimatedRect);

        qDebug() << "Token:" << token.word
                 << "Rect:" << token.estimatedRect
                 << "Distance:" << dist;

        if (dist < minDistance) {
            minDistance = dist;
            closestToken = token;
        }
    }

    qDebug() << "Closest token:" << closestToken.word
             << "Distance:" << minDistance;

    return closestToken;
}

QRect ChineseTokenizer::estimateWordRect(
    int startIndex,
    int endIndex,
    int totalLength,
    const QRect& totalRect)
{
    if (totalLength == 0) {
        return totalRect;
    }

    // 计算词语在文本中的相对位置
    double startRatio = static_cast<double>(startIndex) / totalLength;
    double endRatio = static_cast<double>(endIndex) / totalLength;

    // 假设文本是水平排列的,从左到右
    int wordLeft = totalRect.left() + static_cast<int>(startRatio * totalRect.width());
    int wordRight = totalRect.left() + static_cast<int>(endRatio * totalRect.width());

    // 使用整个矩形的高度
    QRect wordRect(
        wordLeft,
        totalRect.top(),
        wordRight - wordLeft,
        totalRect.height()
        );

    return wordRect;
}

double ChineseTokenizer::distanceToRect(const QPoint& point, const QRect& rect)
{
    // 如果点在矩形内,距离为0
    if (rect.contains(point)) {
        return 0.0;
    }

    // 找到矩形上最近的点
    int closestX = std::clamp(point.x(), rect.left(), rect.right());
    int closestY = std::clamp(point.y(), rect.top(), rect.bottom());

    // 计算欧氏距离
    int dx = point.x() - closestX;
    int dy = point.y() - closestY;

    return std::sqrt(dx * dx + dy * dy);
}
