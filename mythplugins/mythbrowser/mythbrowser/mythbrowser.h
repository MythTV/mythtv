#ifndef MYTHBROWSER_H
#define MYTHBROWSER_H

#include <QUrl>

// MythTV
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuiwebbrowser.h>

#include "bookmarkmanager.h"

class WebPage;

class MythBrowser : public MythScreenType
{
  Q_OBJECT

  public:
    MythBrowser(MythScreenStack *parent, QStringList &urlList);
    ~MythBrowser() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    void setDefaultSaveDirectory(const QString &saveDir) { m_defaultSaveDir = saveDir; }
    void setDefaultSaveFilename(const QString &saveFile) { m_defaultSaveFilename = saveFile; }

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

    void slotEnterURL(void) const;

    void slotAddTab(const QString &url, bool doSwitch = true);
    void slotAddTab() { slotAddTab(""); }
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

    MythUIButtonList         *m_pageList      {nullptr};
    QList<WebPage*>           m_browserList;
    MythUIProgressBar        *m_progressBar   {nullptr};
    MythUIText               *m_titleText     {nullptr};
    MythUIText               *m_statusText    {nullptr};
    MythUIButton             *m_backButton    {nullptr};
    MythUIButton             *m_forwardButton {nullptr};
    MythUIButton             *m_exitButton    {nullptr};

    int       m_currentBrowser                {-1};
    QUrl      m_url;
    QString   m_defaultSaveDir;
    QString   m_defaultSaveFilename;

    Bookmark  m_editBookmark;

    MythDialogBox *m_menuPopup                {nullptr};

    MythImage     *m_defaultFavIcon           {nullptr};

    friend class WebPage;
};

#endif
