// MythTV
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuigroup.h"
#include "mythplayerui.h"
#include "overlays/mythnavigationoverlay.h"
#include "tv_play.h"

MythNavigationOverlay::MythNavigationOverlay(MythMainWindow *MainWindow, TV* Tv,
                                             MythPlayerUI *Player, const QString& Name, OSD* Osd)
  : MythScreenType(static_cast<MythScreenType*>(nullptr), Name),
    m_mainWindow(MainWindow),
    m_tv(Tv),
    m_player(Player),
    m_parentOverlay(Osd)
{
}

bool MythNavigationOverlay::Create()
{
    // Setup screen
    if (!XMLParseBase::LoadWindowFromXML("osd.xml", "osd_navigation", this))
        return false;

    MythUIButton* more = nullptr;
    UIUtilW::Assign(this, more, "more");
    if (more)
        connect(more, &MythUIButton::Clicked, this, &MythNavigationOverlay::More);
    UIUtilW::Assign(this, m_pauseButton,  "PAUSE");
    UIUtilW::Assign(this, m_playButton,   "PLAY");
    UIUtilW::Assign(this, m_muteButton,   "MUTE");
    UIUtilW::Assign(this, m_unMuteButton, "unmute");

    // Listen for player state changes
    connect(m_player, &MythPlayerUI::PauseChanged,      this, &MythNavigationOverlay::PauseChanged);
    connect(m_player, &MythPlayerUI::AudioStateChanged, this, &MythNavigationOverlay::AudioStateChanged);

    // Default constructor uses kMuteOff. Set initial mute button state accordingly
    // and then request latest state.
    if (m_muteButton)
        m_muteButton->Show();
    if (m_unMuteButton)
        m_unMuteButton->Hide();
    m_player->RefreshAudioState();

    // TODO convert to video state when ready
    bool paused = m_player->IsPaused();
    m_paused = !paused;
    PauseChanged(paused);

    // Find number of groups and make sure only corrrect one is visible
    MythUIGroup *group = nullptr;
    for (int i = 0; i < 100 ; i++)
    {
        UIUtilW::Assign(this, group, QString("grp%1").arg(i));
        if (!group)
            break;

        m_maxGroupNum = i;
        if (i != m_visibleGroup)
            group->SetVisible(false);
        QList<MythUIType *> * children = group->GetAllChildren();
        for (auto * child : std::as_const(*children))
        {
            if (child != more)
            {
                auto * button = dynamic_cast<MythUIButton*>(child);
                if (button)
                    connect(button, &MythUIButton::Clicked, this, &MythNavigationOverlay::GeneralAction);
            }
        }
    }

    BuildFocusList();

    return true;
}

bool MythNavigationOverlay::keyPressEvent(QKeyEvent* Event)
{
    bool extendtimeout = true;
    bool handled = false;

    MythUIType* current = GetFocusWidget();
    if (current && current->keyPressEvent(Event))
        handled = true;

    if (!handled)
    {
        QStringList actions;
        handled = m_mainWindow->TranslateKeyPress("qt", Event, actions);

        for (int i = 0; i < actions.size() && !handled; i++)
        {
            const QString& action = actions[i];
            if (action == "ESCAPE" )
            {
                SendResult(-1, action);
                handled = true;
                extendtimeout = false;
            }
        }
    }

    if (!handled && MythScreenType::keyPressEvent(Event))
        handled = true;

    if (extendtimeout)
        m_parentOverlay->SetExpiry(OSD_DLG_NAVIGATE, kOSDTimeout_Long);

    return handled;
}

void MythNavigationOverlay::ShowMenu()
{
    SendResult(100, "MENU");
}

void MythNavigationOverlay::SendResult(int Result, const QString& Action)
{
    auto * dce = new DialogCompletionEvent("", Result, "", Action);
    QCoreApplication::postEvent(m_tv, dce);
}

void MythNavigationOverlay::PauseChanged(bool Paused)
{
    if (!(m_playButton && m_pauseButton))
        return;

    if (m_paused == Paused)
        return;

    m_paused = Paused;
    MythUIType* current = GetFocusWidget();
    m_playButton->SetVisible(m_paused);
    m_pauseButton->SetVisible(!m_paused);

    if (current && (current == m_playButton || current == m_pauseButton))
    {
        current->LoseFocus();
        SetFocusWidget(m_paused ? m_playButton : m_pauseButton);
    }
}

void MythNavigationOverlay::AudioStateChanged(const MythAudioState& AudioState)
{
    if (!(m_muteButton && m_unMuteButton))
        return;

    if (!(AudioState.m_volumeControl && AudioState.m_hasAudioOut))
    {
        m_muteButton->Hide();
        m_unMuteButton->Hide();
    }
    else if (m_audioState.m_muteState != AudioState.m_muteState)
    {
        bool muted = AudioState.m_muteState == kMuteAll;
        MythUIType* current = GetFocusWidget();
        m_muteButton->SetVisible(!muted);
        m_unMuteButton->SetVisible(muted);

        if (current && (current == m_muteButton || current == m_unMuteButton))
        {
            current->LoseFocus();
            SetFocusWidget(muted ? m_unMuteButton : m_muteButton);
        }
    }

    m_audioState = AudioState;
}

void MythNavigationOverlay::GeneralAction()
{
    MythUIType* current = GetFocusWidget();
    if (current)
    {
        QString name = current->objectName();
        int result = 100;
        int hashPos = name.indexOf('#');
        if (hashPos > -1)
            name.truncate(hashPos);
        if (name == "INFO")
            result = 0;
        if (name == "unmute")
            name = "MUTE";
        SendResult(result, name);
    }
}

// Switch to next group of icons. They have to be
// named grp0, grp1, etc with no gaps in numbers.
void MythNavigationOverlay::More()
{
    if (m_maxGroupNum <= 0)
        return;

    MythUIGroup* group = nullptr;
    UIUtilW::Assign(this, group, QString("grp%1").arg(m_visibleGroup));
    if (group != nullptr)
        group->SetVisible(false);

    // wrap around after last group displayed
    if (++m_visibleGroup > m_maxGroupNum)
        m_visibleGroup = 0;

    UIUtilW::Assign(this, group, QString("grp%1").arg(m_visibleGroup));
    if (group != nullptr)
        group->SetVisible(true);
}
