#ifndef MYTHUI_WEBBROWSER_H_
#define MYTHUI_WEBBROWSER_H_

// qt
#include <QtGlobal>
#include <QUrl>
#include <QString>
#include <QElapsedTimer>
#include <QColor>
#include <QIcon>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QWebEngineFullScreenRequest>
//#include <QWebEngineDownloadRequest>

// mythtv
#include "mythuitype.h"
#include "mythuiexp.h"


class MythUIScrollBar;
class MythUIWebBrowser;
class MythUIBusyDialog;
class MythScreenType;

class MythWebEngineView : public QWebEngineView
{
  Q_OBJECT

  public:
    MythWebEngineView(QWidget *parent, MythUIWebBrowser *parentBrowser);
    ~MythWebEngineView(void) override;

    bool eventFilter(QObject *obj, QEvent *event) override;
    void customEvent(QEvent *e) override; // QWidget


  protected slots:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override; // QWebEngineView

  private:
    void sendKeyPress(QKeyEvent *event);
    bool handleKeyPress(QKeyEvent *event);
    void showDownloadMenu(void);
    void openBusyPopup(const QString &message);
    void closeBusyPopup(void);

    static bool isMusicFile(const QString &extension, const QString &mimetype);
    static bool isVideoFile(const QString &extension, const QString &mimetype);

    QString getReplyMimetype(void);
    static QString getExtensionForMimetype(const QString &mimetype);

    QWebEnginePage    *m_webpage         {nullptr};
    MythUIWebBrowser  *m_parentBrowser   {nullptr};
    QWebEngineProfile *m_profile         {nullptr};
    QNetworkRequest    m_downloadRequest;
    QNetworkReply     *m_downloadReply   {nullptr};
    MythUIBusyDialog  *m_busyPopup       {nullptr};
    bool               m_downloadAndPlay {false};
};

/**
 * \brief Web browsing widget. Can load and render local and remote html.
 *        Supports netscape plugins.
 *
 * \ingroup MythUI_Widgets392
 * 
 */
class MUI_PUBLIC MythUIWebBrowser : public MythUIType
{
  Q_OBJECT

  public:
    MythUIWebBrowser(MythUIType *parent, const QString &name);
    ~MythUIWebBrowser() override;

    void Init(void);

    void LoadPage(const QUrl& url);
    void SetHtml(const QString &html, const QUrl &baseUrl = QUrl());

    void LoadUserStyleSheet(const QUrl& url, const QString &name = QString("mythtv"));
    void RemoveUserStyleSheet(const QString &name);

    void SetHttpUserAgent(const QString &userAgent);

    QWebEngineSettings *GetWebEngineSettings(void);
    QWebEngineProfile  *GetWebEngineProfile(void);

    void Pulse(void) override; // MythUIType

    QIcon GetIcon(void);
    QUrl  GetUrl(void);
    QString GetTitle(void);

    void SetActive(bool active);
    bool IsActive(void) const { return m_active; }

    /// returns true if all keypresses are to be passed to the web page
    bool IsInputToggled(void) const { return m_inputToggled; }
    void SetInputToggled(bool inputToggled) { m_inputToggled = inputToggled; }

    void  SetZoom(double zoom);
    float GetZoom(void) const;

    bool CanGoForward(void);
    bool CanGoBack(void);

    void RunJavaScript(const QString& scriptSource);

    void SendStatusBarMessage(const QString &text);

    void SetDefaultSaveDirectory(const QString &saveDir);
    QString GetDefaultSaveDirectory(void) { return m_defaultSaveDir; }

    void SetDefaultSaveFilename(const QString &filename);
    QString GetDefaultSaveFilename(void) { return m_defaultSaveFilename; }

    void HandleMouseAction(const QString &action);

    void UpdateBuffer(void);

    MythScreenType *GetParentScreen(void) { return m_parentScreen; }

  public slots:
    void Back(void);
    void Forward(void);
    void ZoomIn(void);
    void ZoomOut(void);
    void Reload(bool useCache = true);
    void TriggerPageAction(QWebEnginePage::WebAction action, bool checked = false);

  signals:
    void loadStarted(void);                    /// a page is starting to load
    void loadFinished(bool ok);                /// a page has finished loading
    void loadProgress(int progress);           /// % of page loaded
    void titleChanged(const QString &title);   /// a pages title has changed
    void statusBarMessage(const QString &text);/// link hit test messages
    void iconChanged(const QIcon &icon);       /// a pages fav icon has changed
    void iconUrlChanged(const QUrl &url);      /// a pages fav icon has changed
    void fileDownloaded(QString filename);     /// a file has been downloaded

  protected slots:
    void slotLoadStarted(void);
    void slotLoadFinished(bool Ok);
    void slotLoadProgress(int progress);
    void slotTitleChanged(const QString &title);
    void slotStatusBarMessage(const QString &text);
    void slotIconChanged(const QIcon &icon);
    void slotIconUrlChanged(const QUrl &url);
    void slotScrollPositionChanged(QPointF position);
    void slotContentsSizeChanged(QSizeF size);
    void slotLinkClicked(const QUrl &url);
    void slotTopScreenChanged(MythScreenType *screen);
    void slotScrollBarShowing(void);
    void slotScrollBarHiding(void);
    void slotLosingFocus();
    void slotTakingFocus();
    void slotRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode);
    void slotFullScreenRequested(QWebEngineFullScreenRequest fullScreenRequest);
    void slotWindowCloseRequested(void);

  protected:
    void SetBackgroundColor(QColor color);
    void ResetScrollBars(void);
    void UpdateScrollBars(void);
    bool IsOnTopScreen(void);

    void Finalize(void) override; // MythUIType
    void DrawSelf(MythPainter *p, int xoffset, int yoffset, int alphaMod, QRect clipRect) override; // MythUIType
    bool ParseElement(const QString &filename, QDomElement &element, bool showWarnings) override;   // MythUIType
    void CopyFrom(MythUIType *base) override;     // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType

    MythScreenType *m_parentScreen         { nullptr };

    QWebEngineView *m_webEngine            { nullptr };
    MythRect        m_browserArea;
    MythRect        m_actualBrowserArea;

    MythImage    *m_image                  { nullptr };

    bool          m_active                 { false   };
    bool          m_wasActive              { false   };
    bool          m_initialized            { false   };
    QElapsedTimer m_lastUpdateTime;
    int           m_updateInterval         { 0       };

    qreal         m_zoom                   { 1.0     };
    QColor        m_bgColor;
    QUrl          m_widgetUrl;
    QString       m_userCssFile;
    QString       m_defaultSaveDir;
    QString       m_defaultSaveFilename;

    bool          m_inputToggled           { false   };
    QString       m_lastMouseAction;
    int           m_mouseKeyCount          { 0       };
    QElapsedTimer m_lastMouseActionTime;

    MythUIScrollBar *m_horizontalScrollbar { nullptr };
    MythUIScrollBar *m_verticalScrollbar   { nullptr };
};

#endif
