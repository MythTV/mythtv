#ifndef MYTHBROWSER_H
#define MYTHBROWSER_H

#include <QUrl>

#include <mythuiwebbrowser.h>
#include <mythuibuttonlist.h>
#include <mythuibutton.h>
#include <mythscreentype.h>
#include <mythdialogbox.h>
#include <mythuiprogressbar.h>

#include "bookmarkmanager.h"

class WebPage;

class MythBrowser : public MythScreenType
{
  Q_OBJECT

  public:
    MythBrowser(MythScreenStack *parent, QStringList &urlList);
    ~MythBrowser();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void setDefaultSaveDirectory(const QString saveDir) { m_defaultSaveDir = saveDir; }
    void setDefaultSaveFilename(const QString saveFile) { m_defaultSaveFilename = saveFile; }

    MythImage *GetDefaultFavIcon(void)
    {
        if (m_defaultFavIcon)
            m_defaultFavIcon->IncrRef();
        return m_defaultFavIcon;
    }

  public slots:
    void slotOpenURL(const QString &url);

  protected slots:
    void slotZoomIn();
    void slotZoomOut();

    void slotBack();
    void slotForward();

    void slotEnterURL(void);

    void slotAddTab(const QString &url = "", bool doSwitch = true);
    void slotDeleteTab(void);

    void slotAddBookmark(void);

    void slotLoadStarted(void);
    void slotLoadFinished(bool OK);
    void slotLoadProgress(int progress);
    void slotTitleChanged(const QString &title);
    void slotStatusBarMessage(const QString &text);
    void slotTabSelected(MythUIButtonListItem *item);
    void slotTabLosingFocus(void);

  private:
    MythUIWebBrowser* activeBrowser(void);

    void switchTab(int newTab);

    QStringList               m_urlList;

    MythUIButtonList         *m_pageList;
    QList<WebPage*>           m_browserList;
    MythUIProgressBar        *m_progressBar;
    MythUIText               *m_titleText;
    MythUIText               *m_statusText;
    MythUIButton             *m_backButton;
    MythUIButton             *m_forwardButton;
    MythUIButton             *m_exitButton;

    int       m_currentBrowser;
    QUrl      m_url;
    QString   m_defaultSaveDir;
    QString   m_defaultSaveFilename;

    Bookmark  m_editBookmark;

    MythDialogBox *m_menuPopup;

    MythImage     *m_defaultFavIcon;

    friend class WebPage;
};

#endif
