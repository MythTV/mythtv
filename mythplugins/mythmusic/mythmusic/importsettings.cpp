// Qt
#include <QString>

// MythTV
#include <mythcorecontext.h>

#include "importsettings.h"

ImportSettings::ImportSettings(MythScreenStack *parent, const char *name)
        : MythScreenType(parent, name),
        m_paranoiaLevel(NULL),
        m_filenameTemplate(NULL),
        m_noWhitespace(NULL),
        m_postCDRipScript(NULL),
        m_ejectCD(NULL),
        m_encoderType(NULL),
        m_defaultRipQuality(NULL),
        m_mp3UseVBR(NULL),
        m_saveButton(NULL),
        m_cancelButton(NULL)
{
}

ImportSettings::~ImportSettings()
{

}

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
    UIUtilE::Assign(this, m_helpText, "helptext", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'importsettings'");
        return false;
    }

    new MythUIButtonListItem(m_paranoiaLevel, tr("Full"), qVariantFromValue(QString("Full")));
    new MythUIButtonListItem(m_paranoiaLevel, tr("Faster"), qVariantFromValue(QString("Faster")));
    m_paranoiaLevel->SetValueByData(gCoreContext->GetSetting("ParanoiaLevel"));

    m_filenameTemplate->SetText(gCoreContext->GetSetting("FilenameTemplate"));

    int loadNoWhitespace = gCoreContext->GetNumSetting("NoWhitespace", 0);
    if (loadNoWhitespace == 1)
        m_noWhitespace->SetCheckState(MythUIStateType::Full);

    m_postCDRipScript->SetText(gCoreContext->GetSetting("PostCDRipScript"));

    int loadEjectCD = gCoreContext->GetNumSetting("EjectCDAfterRipping", 0);
    if (loadEjectCD == 1)
        m_ejectCD->SetCheckState(MythUIStateType::Full);

    new MythUIButtonListItem(m_encoderType, tr("Ogg Vorbis"), qVariantFromValue(QString("ogg")));
    new MythUIButtonListItem(m_encoderType, tr("Lame (MP3)"), qVariantFromValue(QString("mp3")));
    m_encoderType->SetValueByData(gCoreContext->GetSetting("EncoderType"));

    new MythUIButtonListItem(m_defaultRipQuality, tr("Low"), qVariantFromValue(0));
    new MythUIButtonListItem(m_defaultRipQuality, tr("Medium"), qVariantFromValue(1));
    new MythUIButtonListItem(m_defaultRipQuality, tr("High"), qVariantFromValue(2));
    new MythUIButtonListItem(m_defaultRipQuality, tr("Perfect"), qVariantFromValue(3));
    m_defaultRipQuality->SetValueByData(gCoreContext->GetSetting("DefaultRipQuality"));

    int loadMp3UseVBR = gCoreContext->GetNumSetting("Mp3UseVBR", 0);
    if (loadMp3UseVBR == 1)
        m_mp3UseVBR->SetCheckState(MythUIStateType::Full);

    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));
    connect(m_paranoiaLevel,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_filenameTemplate,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_noWhitespace,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_postCDRipScript,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_ejectCD,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_encoderType,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_defaultRipQuality,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_mp3UseVBR,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));

    BuildFocusList();

    SetFocusWidget(m_paranoiaLevel);

    return true;
}

bool ImportSettings::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
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

    Close();
}

void ImportSettings::slotFocusChanged(void)
{
    if (!m_helpText)
        return;

    QString msg = "";
    if (GetFocusWidget() == m_paranoiaLevel)
        msg = tr("Paranoia level of the CD ripper. Set to "
                 "faster if you're not concerned about "
                 "possible errors in the audio.");
    else if (GetFocusWidget() == m_filenameTemplate)
        msg = tr("Defines the location/name for new songs. Valid tokens are:\n"
                 "GENRE, ARTIST, ALBUM, TRACK, TITLE, YEAR");
    else if (GetFocusWidget() == m_noWhitespace)
        msg = tr("If set, whitespace characters in filenames "
                 "will be replaced with underscore characters.");
    else if (GetFocusWidget() == m_postCDRipScript)
        msg = tr("If present this script will be executed "
                 "after a CD Rip is completed.");
    else if (GetFocusWidget() == m_ejectCD)
        msg = tr("If set, the CD tray will automatically open "
                 "after the CD has been ripped.");
    else if (GetFocusWidget() == m_encoderType)
        msg = tr("Audio encoder to use for CD ripping. "
                 "Note that the quality level 'Perfect' "
                 "will use the FLAC encoder.");
    else if (GetFocusWidget() == m_defaultRipQuality)
        msg = tr("Default quality for new CD rips.");
    else if (GetFocusWidget() == m_mp3UseVBR)
        msg = tr("If set, the MP3 encoder will use variable "
                 "bitrates (VBR) except for the low quality setting. "
                 "The Ogg encoder will always use variable bitrates.");
    else if (GetFocusWidget() == m_cancelButton)
        msg = tr("Exit without saving settings");
    else if (GetFocusWidget() == m_saveButton)
        msg = tr("Save settings and Exit");

    m_helpText->SetText(msg);
}

