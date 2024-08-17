// Qt
#include <QCoreApplication>

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythuibutton.h"
#include "mythchanneloverlay.h"
#include "tv_play.h"

#define LOC QString("ChannelEdit: ")

MythChannelOverlay::MythChannelOverlay(MythMainWindow* MainWindow, TV* Tv, const QString& Name)
  : MythScreenType(static_cast<MythScreenType*>(nullptr), Name),
    m_mainWindow(MainWindow),
    m_tv(Tv)
{
}

bool MythChannelOverlay::Create()
{
    if (!XMLParseBase::LoadWindowFromXML("osd.xml", "ChannelEditor", this))
        return false;

    MythUIButton *probeButton = nullptr;
    MythUIButton *okButton    = nullptr;

    bool err = false;
    UIUtilE::Assign(this, m_callsignEdit, "callsign", &err);
    UIUtilE::Assign(this, m_channumEdit,  "channum",  &err);
    UIUtilE::Assign(this, m_channameEdit, "channame", &err);
    UIUtilE::Assign(this, m_xmltvidEdit,  "XMLTV",    &err);
    UIUtilE::Assign(this, probeButton,    "probe",    &err);
    UIUtilE::Assign(this, okButton,       "ok",       &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'ChannelEditor'");
        return false;
    }

    BuildFocusList();
    connect(okButton,    &MythUIButton::Clicked, this, &MythChannelOverlay::Confirm);
    connect(probeButton, &MythUIButton::Clicked, this, &MythChannelOverlay::Probe);
    SetFocusWidget(okButton);

    return true;
}

void MythChannelOverlay::Confirm()
{
    SendResult(1);
}

void MythChannelOverlay::Probe()
{
    SendResult(2);
}

void MythChannelOverlay::SetText(const InfoMap &Map)
{
    if (Map.contains("callsign"))
        m_callsignEdit->SetText(Map.value("callsign"));
    if (Map.contains("channum"))
        m_channumEdit->SetText(Map.value("channum"));
    if (Map.contains("channame"))
        m_channameEdit->SetText(Map.value("channame"));
    if (Map.contains("XMLTV"))
        m_xmltvidEdit->SetText(Map.value("XMLTV"));
}

void MythChannelOverlay::GetText(InfoMap &Map)
{
    Map["callsign"] = m_callsignEdit->GetText();
    Map["channum"]  = m_channumEdit->GetText();
    Map["channame"] = m_channameEdit->GetText();
    Map["XMLTV"]    = m_xmltvidEdit->GetText();
}

bool MythChannelOverlay::keyPressEvent(QKeyEvent *Event)
{
    if (GetFocusWidget()->keyPressEvent(Event))
        return true;

    QStringList actions;
    bool handled = m_mainWindow->TranslateKeyPress("qt", Event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        if (action == "ESCAPE" )
        {
            SendResult(3);
            handled = true;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(Event))
        handled = true;

    return handled;
}

void MythChannelOverlay::SendResult(int result)
{
    if (!m_tv)
        return;

    QString  message = "";
    switch (result)
    {
        case 1:
            message = "DIALOG_EDITOR_OK_0";
            break;
        case 2:
            message = "DIALOG_EDITOR_PROBE_0";
            break;
        case 3:
            message = "DIALOG_EDITOR_QUIT_0";
            break;
    }

    auto *dce = new DialogCompletionEvent("", result, "", message);
    QCoreApplication::postEvent(m_tv, dce);
}
