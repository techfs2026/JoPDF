#include "dictionaryconnector.h"
#include <QProcess>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QDebug>

DictionaryConnector::DictionaryConnector()
{
    m_goldenDictPath = findGoldenDict();

    if (!m_goldenDictPath.isEmpty()) {
        qInfo() << "DictionaryConnector: Found GoldenDict at" << m_goldenDictPath;
    } else {
        qWarning() << "DictionaryConnector: GoldenDict not found";
    }
}

DictionaryConnector::~DictionaryConnector()
{
}

DictionaryConnector& DictionaryConnector::instance()
{
    static DictionaryConnector instance;
    return instance;
}

bool DictionaryConnector::lookup(const QString& word)
{
    if (word.trimmed().isEmpty()) {
        emit lookupFailed(tr("查询词为空"));
        return false;
    }

    QString program = m_goldenDictPath;
    if (program.isEmpty()) {
        program = findGoldenDict();
        if (program.isEmpty()) {
            emit lookupFailed(tr("未找到GoldenDict，请检查是否已安装"));
            return false;
        }
        m_goldenDictPath = program;
    }

    // 启动GoldenDict
    QStringList args;
    args << word.trimmed();

    bool success = QProcess::startDetached(program, args);

    if (success) {
        qInfo() << "DictionaryConnector: Launched GoldenDict for word:" << word;
        emit lookupStarted(word);
    } else {
        QString error = tr("启动GoldenDict失败: %1").arg(program);
        qWarning() << error;
        emit lookupFailed(error);
    }

    return success;
}

void DictionaryConnector::setGoldenDictPath(const QString& path)
{
    if (!path.isEmpty() && QFile::exists(path)) {
        m_goldenDictPath = path;
        qInfo() << "DictionaryConnector: GoldenDict path set to" << path;
    } else {
        qWarning() << "DictionaryConnector: Invalid path" << path;
    }
}

bool DictionaryConnector::isGoldenDictAvailable() const
{
    if (!m_goldenDictPath.isEmpty() && QFile::exists(m_goldenDictPath)) {
        return true;
    }
    return !findGoldenDict().isEmpty();
}

QString DictionaryConnector::findGoldenDict()
{
    // TODO: 先写死路径，后面配置成设置项

    return QString();
}
