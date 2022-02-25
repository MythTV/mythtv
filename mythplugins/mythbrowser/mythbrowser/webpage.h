#ifndef WEBPAGE_H
#define WEBPAGE_H

// qt
#include <QUrl>

// MythTV
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythscreentype.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuiprogressbar.h>
#include <libmythui/mythuiwebbrowser.h>

class MythBrowser;

class WebPage : public QObject
{
  Q_OBJECT

  public:
    WebPage (MythBrowser *parent, QRect area, const char* name);
    WebPage (MythBrowser *parent, MythUIWebBrowser *browser);

    ~WebPage() override;

    void SetActive(bool active);

    MythUIWebBrowser   *getBrowser()  { return m_browser; }
    MythUIButtonListItem *getListItem() { return m_listItem; }

  signals:
    void loadProgress(int progress);
    void statusBarMessage(const QString &text);
    void loadFinished(bool OK);

  protected slots:
    void slotLoadStarted(void);
    void slotLoadFinished(bool OK);
    void slotLoadProgress(int progress);
    void slotTitleChanged(const QString &title);
    void slotStatusBarMessage(const QString &text);
    void slotIconChanged(void);

  protected:

  private:
    bool                  m_active   {false};

    MythBrowser          *m_parent   {nullptr};
    MythUIWebBrowser     *m_browser  {nullptr};
    MythUIButtonListItem *m_listItem {nullptr};
};

#endif
