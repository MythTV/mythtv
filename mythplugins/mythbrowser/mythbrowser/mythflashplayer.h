#ifndef MYTHFLASHPLAYER_H
#define MYTHFLASHPLAYER_H

#include <libmythui/mythscreentype.h>

class MythUIWebBrowser;

class MythFlashPlayer : public MythScreenType
{
  Q_OBJECT

  public:
    MythFlashPlayer(MythScreenStack *parent, QStringList &urlList);
    ~MythFlashPlayer() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private:
    QVariant evaluateJavaScript(const QString& source);
    MythUIWebBrowser*         m_browser  {nullptr};
    QString                   m_url;
    int                       m_fftime   {30};
    int                       m_rewtime  {5};
    int                       m_jumptime {10};
};

#endif
