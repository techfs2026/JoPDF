#include "textrecognizer.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <QString>
#include <QChar>
#include <QDebug>

namespace RapidOCR {


CTCLabelDecode::CTCLabelDecode(const std::vector<std::string>& character) {
    character_ = getCharacter(character);
    for (size_t i = 0; i < character_.size(); ++i) {
        dict_[character_[i]] = i;
    }
}

CTCLabelDecode::CTCLabelDecode(const std::string& characterPath) {
    character_ = getCharacter(characterPath);
    for (size_t i = 0; i < character_.size(); ++i) {
        dict_[character_[i]] = i;
    }
}

std::vector<std::string> CTCLabelDecode::getCharacter(
    const std::vector<std::string>& character) {

    std::vector<std::string> charList = character;

    // 在末尾插入空格
    insertSpecialChar(charList, " ", charList.size());

    // 在开头插入blank
    insertSpecialChar(charList, "blank", 0);

    return charList;
}

std::vector<std::string> CTCLabelDecode::getCharacter(const std::string& characterPath) {
    std::vector<std::string> charList = readCharacterFile(characterPath);

    // 在末尾插入空格
    insertSpecialChar(charList, " ", charList.size());

    // 在开头插入blank
    insertSpecialChar(charList, "blank", 0);

    return charList;
}

std::vector<std::string> CTCLabelDecode::readCharacterFile(const std::string& path) {
    std::vector<std::string> charList;
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open character file: " + path);
    }

    std::string line;
    while (std::getline(file, line)) {
        // 移除行尾的 \r\n 或 \n
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }

        if (!line.empty()) {
            charList.push_back(line);
        }
    }

    file.close();
    return charList;
}

void CTCLabelDecode::insertSpecialChar(std::vector<std::string>& charList,
                                       const std::string& specialChar,
                                       int loc) {
    if (loc == -1) {
        charList.push_back(specialChar);
    } else {
        charList.insert(charList.begin() + loc, specialChar);
    }
}

std::pair<std::vector<std::pair<std::string, float>>, std::vector<WordInfo>>
CTCLabelDecode::operator()(const cv::Mat& preds,
                           bool returnWordBox,
                           const std::vector<float>& whRatioList,
                           float maxWhRatio) {

    qDebug() << "CTCLabelDecode: Input preds dims=" << preds.dims;
    if (preds.dims >= 1) {
        qDebug() << "  Shape:";
        for (int i = 0; i < preds.dims; ++i) {
            qDebug() << "    dim[" << i << "]=" << preds.size[i];
        }
    }

    cv::Mat preds3D;

    if (preds.dims == 2) {
        // 2D: [seq_len, num_classes] -> 添加batch维度
        qDebug() << "Converting 2D to 3D tensor";
        int seqLen = preds.size[0];
        int numClasses = preds.size[1];

        // 重塑为 [1, seq_len, num_classes]
        int newShape[] = {1, seqLen, numClasses};
        preds3D = preds.reshape(0, 3, newShape);

    } else if (preds.dims == 3) {
        // 已经是3D，直接使用
        preds3D = preds;

    } else if (preds.dims == 4) {
        // 4D: [batch, 1, seq_len, num_classes] -> squeeze掉维度1
        qDebug() << "Converting 4D to 3D tensor";

        int dim0 = preds.size[0];
        int dim1 = preds.size[1];
        int dim2 = preds.size[2];
        int dim3 = preds.size[3];

        if (dim1 == 1) {
            // 移除第二个维度: [batch, 1, seq, cls] -> [batch, seq, cls]
            int batch = dim0;
            int seqLen = dim2;
            int numClasses = dim3;

            int newShape[] = {batch, seqLen, numClasses};
            preds3D = cv::Mat(3, newShape, CV_32F);

            const float* srcPtr = preds.ptr<float>();
            float* dstPtr = preds3D.ptr<float>();

            // 4D索引: [b, 0, s, c]
            // 计算步长
            int stride_b = dim1 * dim2 * dim3;  // batch步长
            int stride_1 = dim2 * dim3;         // dim1步长 (总是0因为dim1=1)
            int stride_s = dim3;                // seq步长
            int stride_c = 1;                   // class步长

            for (int b = 0; b < batch; ++b) {
                for (int s = 0; s < seqLen; ++s) {
                    for (int c = 0; c < numClasses; ++c) {
                        // 4D源索引
                        int srcIdx = b * stride_b + 0 * stride_1 + s * stride_s + c * stride_c;
                        // 3D目标索引
                        int dstIdx = b * (seqLen * numClasses) + s * numClasses + c;

                        dstPtr[dstIdx] = srcPtr[srcIdx];
                    }
                }
            }
        } else {
            throw std::runtime_error("Unsupported 4D tensor shape: dim[1] must be 1");
        }

    } else {
        throw std::runtime_error("Predictions must be 2D, 3D or 4D tensor, got " +
                                 std::to_string(preds.dims) + "D");
    }

    // 现在 preds3D 一定是3D
    int batchSize = preds3D.size[0];
    int seqLen = preds3D.size[1];
    int numClasses = preds3D.size[2];

    qDebug() << "Processed shape: [" << batchSize << "," << seqLen << "," << numClasses << "]";

    // 计算 argmax 和 max
    cv::Mat predsIdx(batchSize, seqLen, CV_32S);
    cv::Mat predsProb(batchSize, seqLen, CV_32F);

    for (int b = 0; b < batchSize; ++b) {
        for (int s = 0; s < seqLen; ++s) {
            float maxVal = -std::numeric_limits<float>::infinity();
            int maxIdx = 0;

            for (int c = 0; c < numClasses; ++c) {

                float val = preds3D.ptr<float>(b)[s * numClasses + c];
                if (val > maxVal) {
                    maxVal = val;
                    maxIdx = c;
                }
            }

            predsIdx.at<int>(b, s) = maxIdx;
            predsProb.at<float>(b, s) = maxVal;
        }
    }

    return decode(predsIdx, predsProb, returnWordBox, whRatioList, maxWhRatio, true);
}

std::pair<std::vector<std::pair<std::string, float>>, std::vector<WordInfo>>
CTCLabelDecode::decode(const cv::Mat& textIndex,
                       const cv::Mat& textProb,
                       bool returnWordBox,
                       const std::vector<float>& whRatioList,
                       float maxWhRatio,
                       bool removeDuplicate) {

    std::vector<std::pair<std::string, float>> resultList;
    std::vector<WordInfo> resultWordsList;

    std::vector<int> ignoredTokens = getIgnoredTokens();
    int batchSize = textIndex.rows;

    for (int batchIdx = 0; batchIdx < batchSize; ++batchIdx) {
        std::vector<int> tokenIndices;
        std::vector<float> tokenProbs;

        // 提取当前批次的索引和概率
        for (int i = 0; i < textIndex.cols; ++i) {
            tokenIndices.push_back(textIndex.at<int>(batchIdx, i));
            tokenProbs.push_back(textProb.at<float>(batchIdx, i));
        }

        // 创建选择掩码
        std::vector<bool> selection(tokenIndices.size(), true);

        // 移除重复
        if (removeDuplicate) {
            for (size_t i = 1; i < tokenIndices.size(); ++i) {
                if (tokenIndices[i] == tokenIndices[i - 1]) {
                    selection[i] = false;
                }
            }
        }

        // 移除忽略的token
        for (size_t i = 0; i < tokenIndices.size(); ++i) {
            for (int ignoredToken : ignoredTokens) {
                if (tokenIndices[i] == ignoredToken) {
                    selection[i] = false;
                    break;
                }
            }
        }

        // 提取置信度
        std::vector<float> confList;
        for (size_t i = 0; i < selection.size(); ++i) {
            if (selection[i]) {
                confList.push_back(tokenProbs[i]);
            }
        }

        // 四舍五入到5位小数
        for (float& conf : confList) {
            conf = std::round(conf * 100000.0f) / 100000.0f;
        }

        if (confList.empty()) {
            confList.push_back(0.0f);
        }

        // 构建字符列表
        std::vector<std::string> charList;
        for (size_t i = 0; i < tokenIndices.size(); ++i) {
            if (selection[i]) {
                int textId = tokenIndices[i];
                if (textId >= 0 && textId < static_cast<int>(character_.size())) {
                    charList.push_back(character_[textId]);
                }
            }
        }

        // 拼接文本
        std::string text;
        for (const auto& ch : charList) {
            text += ch;
        }

        // 计算平均置信度
        float avgConf = 0.0f;
        if (!confList.empty()) {
            avgConf = std::accumulate(confList.begin(), confList.end(), 0.0f) / confList.size();
            avgConf = std::round(avgConf * 100000.0f) / 100000.0f;
        }

        resultList.emplace_back(text, avgConf);

        // 如果需要返回词语框
        if (returnWordBox) {
            WordInfo recWordInfo = getWordInfo(text, selection);

            // 设置行文本长度
            float whRatio = 1.0f;
            if (batchIdx < static_cast<int>(whRatioList.size())) {
                whRatio = whRatioList[batchIdx];
            }
            recWordInfo.lineTxtLen = tokenIndices.size() * whRatio / maxWhRatio;
            recWordInfo.confs = confList;

            resultWordsList.push_back(recWordInfo);
        }
    }

    return {resultList, resultWordsList};
}

WordInfo CTCLabelDecode::getWordInfo(const std::string& text,
                                     const std::vector<bool>& selection) {
    WordInfo wordInfo;

    // 获取有效列索引
    std::vector<int> validCol;
    for (size_t i = 0; i < selection.size(); ++i) {
        if (selection[i]) {
            validCol.push_back(i);
        }
    }

    if (validCol.empty()) {
        return wordInfo;
    }

    // 计算列宽度
    std::vector<float> colWidth(validCol.size(), 0.0f);
    for (size_t i = 1; i < validCol.size(); ++i) {
        colWidth[i] = validCol[i] - validCol[i - 1];
    }

    // 第一个字符的宽度
    QString qText = QString::fromStdString(text);
    if (!qText.isEmpty()) {
        QChar firstChar = qText[0];
        bool isChinese = (firstChar >= QChar(0x4E00) && firstChar <= QChar(0x9FFF));
        colWidth[0] = std::min(isChinese ? 3.0f : 2.0f, static_cast<float>(validCol[0]));
    } else {
        colWidth[0] = std::min(2.0f, static_cast<float>(validCol[0]));
    }

    // 分词
    std::vector<std::string> wordContent;
    std::vector<int> wordColContent;
    WordType currentState = WordType::EN_NUM;
    bool stateInitialized = false;

    size_t charIdx = 0;
    for (const QChar& qch : qText) {
        if (charIdx >= validCol.size()) break;

        std::string ch = QString(qch).toStdString();

        // 跳过空格
        if (qch.isSpace()) {
            if (!wordContent.empty()) {
                wordInfo.words.push_back(wordContent);
                wordInfo.wordCols.push_back(wordColContent);
                wordInfo.wordTypes.push_back(currentState);
                wordContent.clear();
                wordColContent.clear();
            }
            charIdx++;
            continue;
        }

        // 判断字符类型
        bool isChinese = (qch >= QChar(0x4E00) && qch <= QChar(0x9FFF));
        WordType cState = isChinese ? WordType::CN : WordType::EN_NUM;

        if (!stateInitialized) {
            currentState = cState;
            stateInitialized = true;
        }

        // 状态改变或列宽度过大，开始新词
        if (currentState != cState || colWidth[charIdx] > 5.0f) {
            if (!wordContent.empty()) {
                wordInfo.words.push_back(wordContent);
                wordInfo.wordCols.push_back(wordColContent);
                wordInfo.wordTypes.push_back(currentState);
                wordContent.clear();
                wordColContent.clear();
            }
            currentState = cState;
        }

        wordContent.push_back(ch);
        wordColContent.push_back(validCol[charIdx]);
        charIdx++;
    }

    // 添加最后一个词
    if (!wordContent.empty()) {
        wordInfo.words.push_back(wordContent);
        wordInfo.wordCols.push_back(wordColContent);
        wordInfo.wordTypes.push_back(currentState);
    }

    return wordInfo;
}

std::vector<int> CTCLabelDecode::getIgnoredTokens() {
    return {0};  // CTC blank token
}



TextRecognizer::TextRecognizer(const RecognizerConfig& config, OrtInferSession* session)
    : config_(config), session_(session)
{
    if (!session_) {
        throw std::invalid_argument("OrtInferSession pointer cannot be null");
    }

    // 获取字符字典
    std::vector<std::string> character = getCharacterDict();

    // 创建后处理操作
    postprocessOp_ = std::make_unique<CTCLabelDecode>(character);
}

std::vector<std::string> TextRecognizer::getCharacterDict() {
    // 首先尝试从模型元数据中获取
    if (session_->haveKey("character")) {
        return session_->getCharacterList("character");
    }

    // 如果模型中没有，从文件读取
    if (!config_.keysPath.empty()) {
        return CTCLabelDecode::readCharacterFile(config_.keysPath);
    }

    throw std::runtime_error("Character dictionary not found in model or config");
}

TextRecOutput TextRecognizer::operator()(const cv::Mat& img, bool returnWordBox) {
    return (*this)(std::vector<cv::Mat>{img}, returnWordBox);
}

TextRecOutput TextRecognizer::operator()(const std::vector<cv::Mat>& imgList,
                                         bool returnWordBox) {
    auto startTime = std::chrono::high_resolution_clock::now();

    TextRecOutput output;

    if (imgList.empty()) {
        return output;
    }

    // 计算宽高比
    std::vector<float> widthList;
    widthList.reserve(imgList.size());
    for (const auto& img : imgList) {
        float ratio = static_cast<float>(img.cols) / static_cast<float>(img.rows);
        widthList.push_back(ratio);
    }

    // 排序以加速识别
    std::vector<size_t> indices(imgList.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [&widthList](size_t i1, size_t i2) {
                  return widthList[i1] < widthList[i2];
              }
              );

    size_t imgNum = imgList.size();
    std::vector<std::pair<std::pair<std::string, float>, WordInfo>> recRes(imgNum);

    int batchNum = config_.recBatchNum;
    int imgC = config_.recImageShape[0];
    int imgH = config_.recImageShape[1];
    int imgW = config_.recImageShape[2];

    // 分批处理
    for (size_t begImgNo = 0; begImgNo < imgNum; begImgNo += batchNum) {
        size_t endImgNo = std::min(imgNum, begImgNo + batchNum);

        // 计算最大宽高比
        float maxWhRatio = static_cast<float>(imgW) / static_cast<float>(imgH);
        std::vector<float> whRatioList;

        for (size_t ino = begImgNo; ino < endImgNo; ++ino) {
            const cv::Mat& img = imgList[indices[ino]];
            float whRatio = static_cast<float>(img.cols) / static_cast<float>(img.rows);
            maxWhRatio = std::max(maxWhRatio, whRatio);
            whRatioList.push_back(whRatio);
        }

        // 准备批次数据
        std::vector<cv::Mat> normImgBatch;
        for (size_t ino = begImgNo; ino < endImgNo; ++ino) {
            cv::Mat normImg = resizeNormImg(imgList[indices[ino]], maxWhRatio);
            normImgBatch.push_back(normImg);
        }

        // 拼接成批次
        int actualImgW = static_cast<int>(imgH * maxWhRatio);
        std::vector<int> dims = {static_cast<int>(normImgBatch.size()), imgC, imgH, actualImgW};
        cv::Mat batchMat(dims.size(), dims.data(), CV_32F);

        for (size_t i = 0; i < normImgBatch.size(); ++i) {
            size_t offset = i * imgC * imgH * actualImgW;
            float* dstPtr = batchMat.ptr<float>() + offset;
            const float* srcPtr = normImgBatch[i].ptr<float>();
            std::memcpy(dstPtr, srcPtr, imgC * imgH * actualImgW * sizeof(float));
        }

        // 推理
        cv::Mat preds = (*session_)(batchMat);

        // 后处理
        auto [lineResults, wordResults] = (*postprocessOp_)(preds, returnWordBox,
                                                            whRatioList, maxWhRatio);

        // 保存结果
        for (size_t rno = 0; rno < lineResults.size(); ++rno) {
            size_t originalIdx = indices[begImgNo + rno];
            if (returnWordBox) {
                recRes[originalIdx] = {lineResults[rno], wordResults[rno]};
            } else {
                recRes[originalIdx] = {lineResults[rno], WordInfo()};
            }
        }
    }

    // 分离结果
    output.imgs = imgList;
    output.txts.reserve(recRes.size());
    output.scores.reserve(recRes.size());
    output.wordResults.reserve(recRes.size());

    for (const auto& [lineRes, wordRes] : recRes) {
        output.txts.push_back(lineRes.first);
        output.scores.push_back(lineRes.second);
        output.wordResults.push_back(wordRes);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    output.elapse = elapsed.count();

    return output;
}

cv::Mat TextRecognizer::resizeNormImg(const cv::Mat& img, float maxWhRatio) {
    int imgChannel = config_.recImageShape[0];
    int imgHeight = config_.recImageShape[1];
    int imgWidth = static_cast<int>(imgHeight * maxWhRatio);

    if (img.channels() != imgChannel) {
        throw std::runtime_error("Image channel mismatch");
    }

    int h = img.rows;
    int w = img.cols;
    float ratio = static_cast<float>(w) / static_cast<float>(h);

    int resizedW;
    if (std::ceil(imgHeight * ratio) > imgWidth) {
        resizedW = imgWidth;
    } else {
        resizedW = static_cast<int>(std::ceil(imgHeight * ratio));
    }

    // 调整大小
    cv::Mat resizedImage;
    cv::resize(img, resizedImage, cv::Size(resizedW, imgHeight));

    // 转换为float
    resizedImage.convertTo(resizedImage, CV_32F);

    // ✅ 正确的归一化方式（完全匹配Python）
    // transpose((2, 0, 1)) / 255 - 0.5 / 0.5
    std::vector<cv::Mat> channels;
    cv::split(resizedImage, channels);

    // 归一化每个通道
    for (auto& channel : channels) {
        channel /= 255.0f;  // 除以255
        channel -= 0.5f;    // 减0.5
        channel /= 0.5f;    // 除以0.5
    }

    // 创建padding后的图像 [C, H, W]
    cv::Mat result(std::vector<int>{imgChannel, imgHeight, imgWidth}, CV_32F, cv::Scalar(0));

    for (int c = 0; c < imgChannel; ++c) {
        cv::Mat dstChannel(imgHeight, imgWidth, CV_32F,
                           result.ptr<float>() + c * imgHeight * imgWidth);
        cv::Mat srcChannel(imgHeight, resizedW, CV_32F);
        channels[c].copyTo(srcChannel);
        srcChannel.copyTo(dstChannel(cv::Rect(0, 0, resizedW, imgHeight)));
    }

    return result;
}

} // namespace RapidOCR
