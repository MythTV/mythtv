// Qt
#include <QString>

// MythTV
#include <libmythbase/mythcorecontext.h>

// MythMusic
#include "musicplayer.h"
#include "playersettings.h"

bool PlayerSettings::Create()
{
    bool err = false;

    // Load the theme for this screen
    if (!LoadWindowFromXML("musicsettings-ui.xml", "playersettings", this))
        return false;

    UIUtilE::Assign(this, m_resumeMode, "resumemode", &err);
    UIUtilE::Assign(this, m_resumeModeEditor, "resumemodeeditor", &err);
    UIUtilE::Assign(this, m_resumeModeRadio, "resumemoderadio", &err);
    UIUtilE::Assign(this, m_exitAction, "exitaction", &err);
    UIUtilE::Assign(this, m_jumpAction, "jumpaction", &err);
    UIUtilE::Assign(this, m_autoLookupCD, "autolookupcd", &err);
    UIUtilE::Assign(this, m_autoPlayCD, "autoplaycd", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'playersettings'");
        return false;
    }

    new MythUIButtonListItem(m_resumeMode, tr("Off"),   QVariant::fromValue((int)MusicPlayer::RESUME_OFF));
    new MythUIButtonListItem(m_resumeMode, tr("First"), QVariant::fromValue((int)MusicPlayer::RESUME_FIRST));
    new MythUIButtonListItem(m_resumeMode, tr("Track"), QVariant::fromValue((int)MusicPlayer::RESUME_TRACK));
    new MythUIButtonListItem(m_resumeMode, tr("Exact"), QVariant::fromValue((int)MusicPlayer::RESUME_EXACT));
    m_resumeMode->SetValueByData(gCoreContext->GetNumSetting("ResumeModePlayback", MusicPlayer::RESUME_EXACT));

    new MythUIButtonListItem(m_resumeModeEditor, tr("Off"),   QVariant::fromValue((int)MusicPlayer::RESUME_OFF));
    new MythUIButtonListItem(m_resumeModeEditor, tr("First"), QVariant::fromValue((int)MusicPlayer::RESUME_FIRST));
    new MythUIButtonListItem(m_resumeModeEditor, tr("Track"), QVariant::fromValue((int)MusicPlayer::RESUME_TRACK));
    new MythUIButtonListItem(m_resumeModeEditor, tr("Exact"), QVariant::fromValue((int)MusicPlayer::RESUME_EXACT));
    m_resumeModeEditor->SetValueByData(gCoreContext->GetNumSetting("ResumeModeEditor", MusicPlayer::RESUME_OFF));

    new MythUIButtonListItem(m_resumeModeRadio, tr("Off"), QVariant::fromValue((int)MusicPlayer::RESUME_OFF));
    new MythUIButtonListItem(m_resumeModeRadio, tr("On"),  QVariant::fromValue((int)MusicPlayer::RESUME_TRACK));
    m_resumeModeRadio->SetValueByData(gCoreContext->GetNumSetting("ResumeModeRadio", MusicPlayer::RESUME_TRACK));

    new MythUIButtonListItem(m_exitAction, tr("Prompt"), QVariant::fromValue(QString("prompt")));
    new MythUIButtonListItem(m_exitAction, tr("Stop playing"), QVariant::fromValue(QString("stop")));
    new MythUIButtonListItem(m_exitAction, tr("Continue Playing"), QVariant::fromValue(QString("play")));
    m_exitAction->SetValueByData(gCoreContext->GetSetting("MusicExitAction", "prompt"));

    new MythUIButtonListItem(m_jumpAction, tr("Stop playing"), QVariant::fromValue(QString("stop")));
    new MythUIButtonListItem(m_jumpAction, tr("Continue Playing"), QVariant::fromValue(QString("play")));
    m_jumpAction->SetValueByData(gCoreContext->GetSetting("MusicJumpPointAction", "stop"));

    int loadAutoLookupCD = gCoreContext->GetNumSetting("AutoLookupCD", 0);
    if (loadAutoLookupCD == 1)
        m_autoLookupCD->SetCheckState(MythUIStateType::Full);
    int loadAutoPlayCD = gCoreContext->GetNumSetting("AutoPlayCD", 0);
    if (loadAutoPlayCD == 1)
        m_autoPlayCD->SetCheckState(MythUIStateType::Full);

    m_resumeMode->SetHelpText(tr("Playback screen - Resume playback at either the beginning of the "
                 "active play queue, the beginning of the last track played, "
                 "or an exact point within the last track played or not at all."));
    m_resumeModeEditor->SetHelpText(tr("Playlist Editor screen - Resume playback at either the beginning of the "
                 "active play queue, the beginning of the last track played, "
                 "or an exact point within the last track played or not at all."));
    m_resumeModeRadio->SetHelpText(tr("Radio screen - Resume playback at the previous station or not at all"));
    m_exitAction->SetHelpText(tr("Specify what action to take when exiting MythMusic plugin."));
    m_jumpAction->SetHelpText(tr("Specify what action to take when exiting MythMusic plugin due to a jumppoint being executed."));
    m_autoLookupCD->SetHelpText(tr("Automatically lookup an audio CD if it is "
                 "present and show its information in the "
                 "Music Selection Tree."));
    m_autoPlayCD->SetHelpText(tr("Automatically put a new CD on the "
                 "playlist and start playing the CD."));
    m_cancelButton->SetHelpText(tr("Exit without saving settings"));
    m_saveButton->SetHelpText(tr("Save settings and Exit"));

    connect(m_saveButton, &MythUIButton::Clicked, this, &PlayerSettings::slotSave);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    BuildFocusList();

    return true;
}

void PlayerSettings::slotSave(void)
{
    gCoreContext->SaveSetting("ResumeModePlayback", m_resumeMode->GetDataValue().toInt());
    gCoreContext->SaveSetting("ResumeModeEditor", m_resumeModeEditor->GetDataValue().toInt());
    gCoreContext->SaveSetting("ResumeModeRadio", m_resumeModeRadio->GetDataValue().toInt());
    gCoreContext->SaveSetting("MusicExitAction", m_exitAction->GetDataValue().toString());
    gCoreContext->SaveSetting("MusicJumpPointAction", m_jumpAction->GetDataValue().toString());

    int saveAutoLookupCD = (m_autoLookupCD->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("AutoLookupCD", saveAutoLookupCD);
    int saveAutoPlayCD = (m_autoPlayCD->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("AutoPlayCD", saveAutoPlayCD);

    gCoreContext->dispatch(MythEvent(QString("MUSIC_SETTINGS_CHANGED PLAYER_SETTINGS")));

    Close();
}

