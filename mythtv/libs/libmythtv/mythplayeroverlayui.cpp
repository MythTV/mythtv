// MythTV
#include "mythplayeroverlayui.h"

MythPlayerOverlayUI::MythPlayerOverlayUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayerUIBase(MainWindow, Tv, Context, Flags)
{
}

void MythPlayerOverlayUI::UpdateOSDMessage(const QString& Message)
{
    UpdateOSDMessage(Message, kOSDTimeout_Med);
}

void MythPlayerOverlayUI::UpdateOSDMessage(const QString& Message, OSDTimeout Timeout)
{
    m_osdLock.lock();
    if (m_osd)
    {
        InfoMap map;
        map.insert("message_text", Message);
        m_osd->SetText("osd_message", map, Timeout);
    }
    m_osdLock.unlock();
}

void MythPlayerOverlayUI::SetOSDStatus(const QString& title, OSDTimeout timeout)
{
    m_osdLock.lock();
    if (m_osd)
    {
        osdInfo info;
        calcSliderPos(info);
        info.text.insert("title", title);
        m_osd->SetText("osd_status", info.text, timeout);
        m_osd->SetValues("osd_status", info.values, timeout);
    }
    m_osdLock.unlock();
}

void MythPlayerOverlayUI::UpdateOSDStatus(osdInfo &Info, int Type, OSDTimeout Timeout)
{
    m_osdLock.lock();
    if (m_osd)
    {
        m_osd->ResetWindow("osd_status");
        m_osd->SetValues("osd_status", Info.values, Timeout);
        m_osd->SetText("osd_status",   Info.text, Timeout);
        if (Type != kOSDFunctionalType_Default)
            m_osd->SetFunctionalWindow("osd_status", static_cast<OSDFunctionalType>(Type));
    }
    m_osdLock.unlock();
}

void MythPlayerOverlayUI::UpdateOSDStatus(const QString& Title, const QString& Desc,
                                          const QString& Value, int Type, const QString& Units,
                                          int Position, OSDTimeout Timeout)
{
    osdInfo info;
    info.values.insert("position", Position);
    info.values.insert("relposition", Position);
    info.text.insert("title", Title);
    info.text.insert("description", Desc);
    info.text.insert("value", Value);
    info.text.insert("units", Units);
    UpdateOSDStatus(info, Type, Timeout);
}
