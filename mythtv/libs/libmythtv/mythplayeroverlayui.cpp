// MythTV
#include "mythplayeroverlayui.h"

MythPlayerOverlayUI::MythPlayerOverlayUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayerUIBase(MainWindow, Tv, Context, Flags)
{
}

void MythPlayerOverlayUI::UpdateOSDMessage(const QString& Message)
{
    m_osdLock.lock();
    if (m_osd)
    {
        InfoMap map;
        map.insert("message_text", Message);
        m_osd->SetText("osd_message", map, kOSDTimeout_Med);
    }
    m_osdLock.unlock();
}
