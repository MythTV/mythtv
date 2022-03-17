#ifndef MYTHCHANNELOVERLAY_H
#define MYTHCHANNELOVERLAY_H

// MythTV
#include "libmythui/mythscreentype.h"

class TV;
class MythMainWindow;

class MythChannelOverlay : public MythScreenType
{
    Q_OBJECT

  public:
    MythChannelOverlay(MythMainWindow* MainWindow, TV* Tv, const QString& Name);
    bool Create() override;
    bool keyPressEvent(QKeyEvent* Event) override;
    void SetText(const InfoMap& Map);
    void GetText(InfoMap& Map);

  public slots:
    void Confirm();
    void Probe();

  protected:
    void SendResult(int result);

    MythUITextEdit* m_callsignEdit { nullptr };
    MythUITextEdit* m_channumEdit  { nullptr };
    MythUITextEdit* m_channameEdit { nullptr };
    MythUITextEdit* m_xmltvidEdit  { nullptr };
    MythMainWindow* m_mainWindow   { nullptr };
    TV*             m_tv           { nullptr };
};

#endif
