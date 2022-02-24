// Qt
#include <QString>

// MythTV
#include <libmythbase/mythcorecontext.h>

// MythMusic
#include "importsettings.h"

bool ImportSettings::Create()
{
    bool err = false;

    // Load the theme for this screen
    if (!LoadWindowFromXML("musicsettings-ui.xml", "importsettings", this))
        return false;

    UIUtilE::Assign(this, m_paranoiaLevel, "paranoialevel", &err);
    UIUtilE::Assign(this, m_filenameTemplate, "filenametemplate", &err);
    UIUtilE::Assign(this, m_noWhitespace, "nowhitespace", &err);
    UIUtilE::Assign(this, m_postCDRipScript, "postcdripscript", &err);
    UIUtilE::Assign(this, m_ejectCD, "ejectcd", &err);
    UIUtilE::Assign(this, m_encoderType, "encodertype", &err);
    UIUtilE::Assign(this, m_defaultRipQuality, "defaultripquality", &err);
    UIUtilE::Assign(this, m_mp3UseVBR, "mp3usevbr", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'importsettings'");
        return false;
    }

    new MythUIButtonListItem(m_paranoiaLevel, tr("Full"), QVariant::fromValue(QString("Full")));
    new MythUIButtonListItem(m_paranoiaLevel, tr("Faster"), QVariant::fromValue(QString("Faster")));
    m_paranoiaLevel->SetValueByData(gCoreContext->GetSetting("ParanoiaLevel"));

    m_filenameTemplate->SetText(gCoreContext->GetSetting("FilenameTemplate"));

    int loadNoWhitespace = gCoreContext->GetNumSetting("NoWhitespace", 0);
    if (loadNoWhitespace == 1)
        m_noWhitespace->SetCheckState(MythUIStateType::Full);

    m_postCDRipScript->SetText(gCoreContext->GetSetting("PostCDRipScript"));

    int loadEjectCD = gCoreContext->GetNumSetting("EjectCDAfterRipping", 0);
    if (loadEjectCD == 1)
        m_ejectCD->SetCheckState(MythUIStateType::Full);

    new MythUIButtonListItem(m_encoderType, tr("Ogg Vorbis"), QVariant::fromValue(QString("ogg")));
    new MythUIButtonListItem(m_encoderType, tr("Lame (MP3)"), QVariant::fromValue(QString("mp3")));
    m_encoderType->SetValueByData(gCoreContext->GetSetting("EncoderType"));

    new MythUIButtonListItem(m_defaultRipQuality, tr("Low"), QVariant::fromValue(0));
    new MythUIButtonListItem(m_defaultRipQuality, tr("Medium"), QVariant::fromValue(1));
    new MythUIButtonListItem(m_defaultRipQuality, tr("High"), QVariant::fromValue(2));
    new MythUIButtonListItem(m_defaultRipQuality, tr("Perfect"), QVariant::fromValue(3));
    m_defaultRipQuality->SetValueByData(gCoreContext->GetSetting("DefaultRipQuality"));

    int loadMp3UseVBR = gCoreContext->GetNumSetting("Mp3UseVBR", 0);
    if (loadMp3UseVBR == 1)
        m_mp3UseVBR->SetCheckState(MythUIStateType::Full);

    connect(m_saveButton, &MythUIButton::Clicked, this, &ImportSettings::slotSave);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

    m_paranoiaLevel->SetHelpText(tr("Paranoia level of the CD ripper. Set to "
                 "faster if you're not concerned about "
                 "possible errors in the audio."));
    m_filenameTemplate->SetHelpText(tr("Defines the location/name for new "
                 "songs. Valid tokens are:\n"
                 "GENRE, ARTIST, ALBUM, TRACK, TITLE, YEAR"));
    m_noWhitespace->SetHelpText(tr("If set, whitespace characters in filenames "
                 "will be replaced with underscore characters."));
    m_postCDRipScript->SetHelpText(tr("If present this script will be executed "
                 "after a CD Rip is completed."));
    m_ejectCD->SetHelpText(tr("If set, the CD tray will automatically open "
                 "after the CD has been ripped."));
    m_encoderType->SetHelpText(tr("Audio encoder to use for CD ripping. "
                 "Note that the quality level 'Perfect' "
                 "will use the FLAC encoder."));
    m_defaultRipQuality->SetHelpText(tr("Default quality for new CD rips."));
    m_mp3UseVBR->SetHelpText(tr("If set, the MP3 encoder will use variable "
                 "bitrates (VBR) except for the low quality setting. "
                 "The Ogg encoder will always use variable bitrates."));
    m_cancelButton->SetHelpText(tr("Exit without saving settings"));
    m_saveButton->SetHelpText(tr("Save settings and Exit"));

    BuildFocusList();

    SetFocusWidget(m_paranoiaLevel);

    return true;
}

bool ImportSettings::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    return MythScreenType::keyPressEvent(event);
}

void ImportSettings::slotSave(void)
{
    gCoreContext->SaveSetting("ParanoiaLevel", m_paranoiaLevel->GetDataValue().toString());
    gCoreContext->SaveSetting("FilenameTemplate", m_filenameTemplate->GetText());
    gCoreContext->SaveSetting("PostCDRipScript", m_postCDRipScript->GetText());
    gCoreContext->SaveSetting("EncoderType", m_encoderType->GetDataValue().toString());
    gCoreContext->SaveSetting("DefaultRipQuality", m_defaultRipQuality->GetDataValue().toString());

    int saveNoWhitespace = (m_noWhitespace->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("Ignore_ID3", saveNoWhitespace);

    int saveEjectCD = (m_ejectCD->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("EjectCDAfterRipping", saveEjectCD);

    int saveMp3UseVBR = (m_mp3UseVBR->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("Mp3UseVBR", saveMp3UseVBR);

    gCoreContext->dispatch(MythEvent(QString("MUSIC_SETTINGS_CHANGED IMPORT_SETTINGS")));

    Close();
}
