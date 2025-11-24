#ifndef PDFCONTENTHANDLER_H
#define PDFCONTENTHANDLER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <memory>

class MuPDFRenderer;
class OutlineManager;
class ThumbnailManager;
class OutlineItem;
class OutlineEditor;

/**
 * @brief PDF 内容处理器
 *
 * 负责管理 PDF 文档的核心内容功能：
 * - 文档加载与关闭
 * - 大纲（Outline）管理
 * - 缩略图（Thumbnail）管理
 */
class PDFContentHandler : public QObject
{
    Q_OBJECT

public:
    explicit PDFContentHandler(MuPDFRenderer* renderer, QObject* parent = nullptr);
    ~PDFContentHandler();

    // ========== 文档加载 ==========

    /**
     * @brief 加载 PDF 文档
     * @param filePath PDF 文件路径
     * @param errorMessage 错误信息输出参数
     * @return 成功返回 true，失败返回 false
     */
    bool loadDocument(const QString& filePath, QString* errorMessage = nullptr);

    /**
     * @brief 关闭当前文档
     */
    void closeDocument();

    /**
     * @brief 检查是否有文档已加载
     */
    bool isDocumentLoaded() const;

    /**
     * @brief 获取文档页数
     */
    int pageCount() const;

    // ========== 大纲管理 ==========

    /**
     * @brief 加载文档大纲
     * @return 成功返回 true，失败返回 false
     */
    bool loadOutline();

    /**
     * @brief 获取大纲根节点
     */
    OutlineItem* outlineRoot() const;

    /**
     * @brief 获取大纲项总数
     */
    int outlineItemCount() const;

    /**
     * @brief 检查是否有大纲
     */
    bool hasOutline() const;

    /**
     * @brief 清空大纲数据
     */
    void clearOutline();

    // ========== 缩略图管理 (新版智能管理器) ==========

    /**
     * @brief 获取缩略图管理器
     */
    ThumbnailManager* thumbnailManager() const { return m_thumbnailManager.get(); }

    /**
     * @brief 获取缩略图（优先高清，其次低清）
     */
    QImage getThumbnail(int pageIndex, bool preferHighRes = true) const;

    /**
     * @brief 检查是否有缩略图
     */
    bool hasThumbnail(int pageIndex) const;

    /**
     * @brief 设置缩略图尺寸
     */
    void setThumbnailSize(int lowResWidth, int highResWidth);

    /**
     * @brief 设置缩略图旋转角度
     */
    void setThumbnailRotation(int rotation);

    /**
     * @brief 立即渲染低清缩略图（同步）
     */
    void renderLowResImmediate(const QVector<int>& pageIndices);

    /**
     * @brief 异步渲染高清缩略图
     */
    void renderHighResAsync(const QVector<int>& pageIndices, int priority = 1);

    /**
     * @brief 异步渲染低清缩略图
     */
    void renderLowResAsync(const QVector<int>& pageIndices);

    /**
     * @brief 取消所有缩略图渲染任务
     */
    void cancelThumbnailTasks();

    /**
     * @brief 清空缩略图缓存
     */
    void clearThumbnails();

    /**
     * @brief 获取缩略图统计信息
     */
    QString getThumbnailStatistics() const;

    /**
     * @brief 获取已缓存的缩略图数量
     */
    int cachedThumbnailCount() const;

    // ========== 工具方法 ==========

    /**
     * @brief 检测 PDF 类型（文本/扫描）
     * @param samplePages 采样页数
     * @return true 为文本 PDF，false 为扫描 PDF
     */
    bool isTextPDF(int samplePages = 5) const;

    /**
     * @brief 重置所有状态
     */
    void reset();

    // ========== 大纲编辑 ==========

    /**
     * @brief 获取大纲编辑器
     * @return 编辑器指针(不转移所有权)
     */
    OutlineEditor* outlineEditor() const { return m_outlineEditor.get(); }

    /**
     * @brief 添加大纲项(便捷方法)
     */
    OutlineItem* addOutlineItem(OutlineItem* parent, const QString& title,
                                int pageIndex, int insertIndex = -1);

    /**
     * @brief 删除大纲项(便捷方法)
     */
    bool deleteOutlineItem(OutlineItem* item);

    /**
     * @brief 重命名大纲项(便捷方法)
     */
    bool renameOutlineItem(OutlineItem* item, const QString& newTitle);

    /**
     * @brief 保存大纲修改
     */
    bool saveOutlineChanges(const QString& savePath = QString());

    /**
     * @brief 是否有未保存的大纲修改
     */
    bool hasUnsavedOutlineChanges() const;

signals:
    // ========== 文档信号 ==========

    /**
     * @brief 文档加载完成
     * @param filePath 文件路径
     * @param pageCount 页数
     */
    void documentLoaded(const QString& filePath, int pageCount);

    /**
     * @brief 文档关闭
     */
    void documentClosed();

    /**
     * @brief 文档加载错误
     * @param errorMessage 错误信息
     */
    void documentError(const QString& errorMessage);

    // ========== 大纲信号 ==========

    /**
     * @brief 大纲加载完成
     * @param success 是否成功
     * @param itemCount 大纲项数量
     */
    void outlineLoaded(bool success, int itemCount);

    // ========== 缩略图信号 (新版) ==========

    /**
     * @brief 缩略图已加载（低清或高清）
     */
    void thumbnailLoaded(int pageIndex, const QImage& thumbnail, bool isHighRes);

    /**
     * @brief 缩略图加载进度
     */
    void thumbnailLoadProgress(int loaded, int total);

    // ========== 大纲编辑信号 ==========

    void outlineModified();
    void outlineSaveCompleted(bool success, const QString& errorMsg);

private:
    MuPDFRenderer* m_renderer;
    std::unique_ptr<OutlineManager> m_outlineManager;
    std::unique_ptr<ThumbnailManager> m_thumbnailManager;
    std::unique_ptr<OutlineEditor> m_outlineEditor;

    // 连接子管理器的信号
    void setupConnections();
};

#endif // PDFCONTENTHANDLER_H
