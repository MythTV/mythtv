#ifndef WEBPAGE_H
#define WEBPAGE_H

// qt
#include <QUrl>

// myth
#include <libmythui/mythuiwebbrowser.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythuiprogressbar.h>

class MythBrowser;

class WebPage : public QObject
{
  Q_OBJECT

  public:
    WebPage (MythBrowser *parent, QRect area, const char* name);
    WebPage (MythBrowser *parent, MythUIWebBrowser *browser);

    ~WebPage();

    void SetActive(bool active);

    MythUIWebBrowser   *getBrowser()  { return m_browser; }
    MythUIButtonListItem *getListItem() { return m_listItem; }

  signals:
    void loadProgress(int progress);
    void statusBarMessage(const QString &text);

  protected slots:
    void slotLoadStarted(void);
    void slotLoadFinished(bool OK);
    void slotLoadProgress(int progress);
    void slotTitleChanged(const QString &title);
    void slotStatusBarMessage(const QString &text);
    void slotIconChanged(void);

  protected:

  private:
    bool                m_active;

    MythBrowser        *m_parent;
    MythUIWebBrowser   *m_browser;
    MythUIButtonListItem *m_listItem;
};

#endif
