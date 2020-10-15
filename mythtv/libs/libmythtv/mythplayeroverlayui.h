#ifndef MYTHPLAYEROVERLAYUI_H
#define MYTHPLAYEROVERLAYUI_H

// MythTV
#include "mythplayeruibase.h"

class MythPlayerOverlayUI : public MythPlayerUIBase
{
    Q_OBJECT

  public:
    MythPlayerOverlayUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);
   ~MythPlayerOverlayUI() override = default;

  protected slots:
    void UpdateOSDMessage (const QString& Message);
    void UpdateOSDMessage (const QString& Message, OSDTimeout Timeout) override;
    void SetOSDStatus     (const QString &Title, OSDTimeout Timeout);
    void UpdateOSDStatus  (osdInfo &Info, int Type, enum OSDTimeout Timeout);
    void UpdateOSDStatus  (const QString& Title, const QString& Desc,
                           const QString& Value, int Type, const QString& Units,
                           int Position, OSDTimeout Timeout);

  private:
    Q_DISABLE_COPY(MythPlayerOverlayUI)
};

#endif
