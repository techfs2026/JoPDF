#ifndef DICTIONARYCONNECTOR_H
#define DICTIONARYCONNECTOR_H

#include <QObject>
#include <QString>

/**
 * @brief 词典连接器 - 调用外部词典程序
 *
 * 全局单例，与OCR功能配合使用
 */
class DictionaryConnector : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static DictionaryConnector& instance();

    /**
     * @brief 查词（调用GoldenDict）
     * @param word 要查的单词
     * @return 是否成功启动词典
     */
    bool lookup(const QString& word);

    /**
     * @brief 设置GoldenDict路径
     */
    void setGoldenDictPath(const QString& path);

    /**
     * @brief 检查GoldenDict是否可用
     */
    bool isGoldenDictAvailable() const;

    /**
     * @brief 查找GoldenDict程序
     */
    static QString findGoldenDict();

signals:
    /**
     * @brief 词典启动成功
     */
    void lookupStarted(const QString& word);

    /**
     * @brief 词典启动失败
     */
    void lookupFailed(const QString& error);

private:
    DictionaryConnector();
    ~DictionaryConnector();
    DictionaryConnector(const DictionaryConnector&) = delete;
    DictionaryConnector& operator=(const DictionaryConnector&) = delete;

private:
    QString m_goldenDictPath;
};

#endif // DICTIONARYCONNECTOR_H
