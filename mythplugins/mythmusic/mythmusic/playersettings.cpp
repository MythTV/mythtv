// Qt
#include <QString>

// MythTV
#include <mythcorecontext.h>

#include "playersettings.h"
#include "musicplayer.h"

PlayerSettings::PlayerSettings(MythScreenStack *parent, const char *name)
        : MythScreenType(parent, name),
        m_resumeMode(NULL),
        m_resumeModeEditor(),
        m_resumeModeRadio(),
        m_exitAction(NULL),
        m_autoLookupCD(NULL),
        m_autoPlayCD(NULL),
        m_saveButton(NULL),
        m_cancelButton(NULL)
{
}

PlayerSettings::~PlayerSettings()
{

}

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
    UIUtilE::Assign(this, m_autoLookupCD, "autolookupcd", &err);
    UIUtilE::Assign(this, m_autoPlayCD, "autoplaycd", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'playersettings'");
        return false;
    }

    new MythUIButtonListItem(m_resumeMode, tr("Off"),   qVariantFromValue((int)MusicPlayer::RESUME_OFF));
    new MythUIButtonListItem(m_resumeMode, tr("First"), qVariantFromValue((int)MusicPlayer::RESUME_FIRST));
    new MythUIButtonListItem(m_resumeMode, tr("Track"), qVariantFromValue((int)MusicPlayer::RESUME_TRACK));
    new MythUIButtonListItem(m_resumeMode, tr("Exact"), qVariantFromValue((int)MusicPlayer::RESUME_EXACT));
    m_resumeMode->SetValueByData(gCoreContext->GetNumSetting("ResumeModePlayback", MusicPlayer::RESUME_EXACT));

    new MythUIButtonListItem(m_resumeModeEditor, tr("Off"),   qVariantFromValue((int)MusicPlayer::RESUME_OFF));
    new MythUIButtonListItem(m_resumeModeEditor, tr("First"), qVariantFromValue((int)MusicPlayer::RESUME_FIRST));
    new MythUIButtonListItem(m_resumeModeEditor, tr("Track"), qVariantFromValue((int)MusicPlayer::RESUME_TRACK));
    new MythUIButtonListItem(m_resumeModeEditor, tr("Exact"), qVariantFromValue((int)MusicPlayer::RESUME_EXACT));
    m_resumeModeEditor->SetValueByData(gCoreContext->GetNumSetting("ResumeModeEditor", MusicPlayer::RESUME_OFF));

    new MythUIButtonListItem(m_resumeModeRadio, tr("Off"), qVariantFromValue((int)MusicPlayer::RESUME_OFF));
    new MythUIButtonListItem(m_resumeModeRadio, tr("On"),  qVariantFromValue((int)MusicPlayer::RESUME_TRACK));
    m_resumeModeRadio->SetValueByData(gCoreContext->GetNumSetting("ResumeModeRadio", MusicPlayer::RESUME_TRACK));

    new MythUIButtonListItem(m_exitAction, tr("Prompt"), qVariantFromValue(QString("prompt")));
    new MythUIButtonListItem(m_exitAction, tr("Stop playing"), qVariantFromValue(QString("stop")));
    new MythUIButtonListItem(m_exitAction, tr("Continue Playing"), qVariantFromValue(QString("play")));
    m_exitAction->SetValueByData(gCoreContext->GetSetting("MusicExitAction"));

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
    m_autoLookupCD->SetHelpText(tr("Automatically lookup an audio CD if it is "
                 "present and show its information in the "
                 "Music Selection Tree."));
    m_autoPlayCD->SetHelpText(tr("Automatically put a new CD on the "
                 "playlist and start playing the CD."));
    m_cancelButton->SetHelpText(tr("Exit without saving settings"));
    m_saveButton->SetHelpText(tr("Save settings and Exit"));

    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    BuildFocusList();

    return true;
}

void PlayerSettings::slotSave(void)
{
    gCoreContext->SaveSetting("ResumeModePlayback", m_resumeMode->GetDataValue().toInt());
    gCoreContext->SaveSetting("ResumeModeEditor", m_resumeModeEditor->GetDataValue().toInt());
    gCoreContext->SaveSetting("ResumeModeRadio", m_resumeModeRadio->GetDataValue().toInt());
    gCoreContext->SaveSetting("MusicExitAction", m_exitAction->GetDataValue().toString());

    int saveAutoLookupCD = (m_autoLookupCD->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("AutoLookupCD", saveAutoLookupCD);
    int saveAutoPlayCD = (m_autoPlayCD->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("AutoPlayCD", saveAutoPlayCD);

    gCoreContext->dispatch(MythEvent(QString("MUSIC_SETTINGS_CHANGED PLAYER_SETTINGS")));

    Close();
}

