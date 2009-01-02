#ifndef MYTHUI_WEBBROWSER_H_
#define MYTHUI_WEBBROWSER_H_

#include <QtGlobal>
#include <QString>
#include <QUrl>
#include <QTime>
#include <QColor>
#include <QIcon>


#ifdef USING_QTWEBKIT
#include <QWebView>
#include <QWebPage>
#include <QNetworkReply>
#endif

#include "mythuitype.h"


class MythUIWebBrowser;

#ifdef USING_QTWEBKIT
class MythWebView : public QWebView
{
  Q_OBJECT

  public:
    MythWebView(QWidget *parent, MythUIWebBrowser *parentBrowser);

    virtual void keyPressEvent(QKeyEvent *event);
  protected slots:
    void  handleUnsupportedContent(QNetworkReply *reply);
  private:
    MythUIWebBrowser *m_parentBrowser;
};
#endif

class MPUBLIC MythUIWebBrowser : public MythUIType
{
  Q_OBJECT

  public:
    MythUIWebBrowser(MythUIType *parent, const QString &name);
    ~MythUIWebBrowser();

    void Init(void);

    void LoadPage(QUrl url);
    void SetHtml(const QString &html, const QUrl &baseUrl = QUrl());

    virtual bool keyPressEvent(QKeyEvent *event);

    QIcon GetIcon(void);
    QUrl  GetUrl(void);

    void SetActive(bool active);
    bool IsActive(void) { return m_active; }

    void SetZoom(float zoom);

    bool CanGoForward(void);
    bool CanGoBack(void);

  public slots:
    void Back(void);
    void Forward(void);
    void ZoomIn(void);
    void ZoomOut(void);

  signals:
    void loadStarted(void);                    /// a page is starting to load
    void loadFinished(bool ok);                /// a page has finished loading
    void loadProgress(int progress);           /// % of page loaded
    void titleChanged(const QString &title);   /// a pages title has changed
    void statusBarMessage(const QString &text);/// link hit test messages
    void iconChanged(void);                    /// a pages fav icon has changed

  protected slots:
    void slotLoadStarted(void);
    void slotLoadFinished(bool Ok);
    void slotLoadProgress(int progress);
    void slotTitleChanged(const QString &title);
    void slotTakingFocus(void);
    void slotLosingFocus(void);
    void slotStatusBarMessage(const QString &text);
    void slotIconChanged(void);

  protected:
    void Finalize(void);
    void UpdateBuffer(void);
    void HandleMouseAction(const QString &action);
    void SetBackgroundColor(QColor color);

    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRegion);

    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

#ifdef USING_QTWEBKIT
    MythWebView *m_browser;
#endif

    MythImage   *m_image;

    bool         m_active;
    bool         m_initialized;
    float        m_zoom;
    QColor       m_bgColor;

    bool         m_inputToggled;
    QString      m_lastMouseAction;
    int          m_mouseKeyCount;
    QTime        m_lastMouseActionTime;
};

#endif
