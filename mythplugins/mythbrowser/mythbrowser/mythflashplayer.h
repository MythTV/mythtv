#ifndef MYTHFLASHPLAYER_H
#define MYTHFLASHPLAYER_H

#include <mythscreentype.h>

class MythUIWebBrowser;

class MythFlashPlayer : public MythScreenType
{
  Q_OBJECT

  public:
    MythFlashPlayer(MythScreenStack *parent, QStringList &urlList);
    ~MythFlashPlayer();

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *) override; // MythScreenType

  private:
    QVariant evaluateJavaScript(const QString&);
    MythUIWebBrowser*         m_browser;
    QString                   m_url;
    int                       m_fftime;
    int                       m_rewtime;
    int                       m_jumptime;
};

#endif
