#ifndef PDFDOCUMENTTAB_H
#define PDFDOCUMENTTAB_H

#include <QWidget>
#include <QSplitter>

#include "datastructure.h"
#include "pdfpagewidget.h"
#include "searchwidget.h"
#include "navigationpanel.h"

class PDFDocumentSession;
class PDFPageWidget;
class SearchWidget;
class QScrollArea;
class QProgressBar;

/**
 * @brief 单个PDF文档标签页
 *
 * 职责:
 * - 管理单个PDF文档的完整视图(Session + PageWidget + NavigationPanel)
 * - 处理标签页内的布局和交互
 * - 向MainWindow报告状态变化
 */
class PDFDocumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit PDFDocumentTab(QWidget* parent = nullptr);
    ~PDFDocumentTab();

    // ==================== 文档操作 ====================
    bool loadDocument(const QString& filePath, QString* errorMessage = nullptr);
    void closeDocument();
    bool isDocumentLoaded() const;
    QString documentPath() const;
    QString documentTitle() const; // 用于标签页标题

    // ==================== 组件访问 ====================
    PDFDocumentSession* session() const { return m_session; }
    PDFPageWidget* pageWidget() const { return m_pageWidget; }
    NavigationPanel* navigationPanel() const { return m_navigationPanel; }
    SearchWidget* searchWidget() const { return m_searchWidget; }

    // ==================== 导航操作 ====================
    void previousPage();
    void nextPage();
    void firstPage();
    void lastPage();
    void goToPage(int pageIndex);

    // ==================== 缩放操作 ====================
    void zoomIn();
    void zoomOut();
    void actualSize();
    void fitPage();
    void fitWidth();
    void setZoom(double zoom);

    // ==================== 视图操作 ====================
    void setDisplayMode(PageDisplayMode mode);
    void setContinuousScroll(bool continuous);

    // ==================== 搜索操作 ====================
    void showSearchBar();
    void hideSearchBar();
    bool isSearchBarVisible() const;

    // ==================== 文本操作 ====================
    void copySelectedText();
    void selectAll();

    // ==================== 链接操作 ====================
    void setLinksVisible(bool visible);
    bool linksVisible() const;

    // ==================== 状态查询 ====================
    int currentPage() const;
    int pageCount() const;
    double zoom() const;
    ZoomMode zoomMode() const;
    PageDisplayMode displayMode() const;
    bool isContinuousScroll() const;
    bool hasTextSelection() const;
    bool isTextPDF() const;

signals:
    // 文档生命周期
    void documentLoaded(const QString& filePath, int pageCount);
    void documentClosed();
    void documentError(const QString& error);

    // 视图状态变化 - 用于更新MainWindow的UI
    void pageChanged(int pageIndex);
    void zoomChanged(double zoom);
    void displayModeChanged(PageDisplayMode mode);
    void continuousScrollChanged(bool continuous);
    void textSelectionChanged();

    // 搜索相关
    void searchCompleted(const QString& query, int totalMatches);

    // 进度相关
    void textPreloadProgress(int current, int total);
    void textPreloadCompleted();

private slots:
    void onDocumentLoaded(const QString& filePath, int pageCount);
    void onPageChanged(int pageIndex);
    void onTextPreloadProgress(int current, int total);
    void onTextPreloadCompleted();

private:
    void setupUI();
    void setupConnections();
    void updateScrollBarPolicy();

private:
    // 核心组件
    PDFDocumentSession* m_session;
    PDFPageWidget* m_pageWidget;
    NavigationPanel* m_navigationPanel;
    SearchWidget* m_searchWidget;

    // UI组件
    QSplitter* m_splitter;
    QScrollArea* m_scrollArea;
    QProgressBar* m_textPreloadProgress;
};

#endif // PDFDOCUMENTTAB_H
