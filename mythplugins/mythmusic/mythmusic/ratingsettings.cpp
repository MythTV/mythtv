// Qt
#include <QString>

// MythTV
#include <mythcorecontext.h>

#include "ratingsettings.h"

RatingSettings::RatingSettings(MythScreenStack *parent, const char *name)
        : MythScreenType(parent, name),
        m_ratingWeight(NULL), m_playCountWeight(NULL),
        m_lastPlayWeight(NULL), m_randomWeight(NULL),
        m_saveButton(NULL), m_cancelButton(NULL)
{
}

RatingSettings::~RatingSettings()
{

}

bool RatingSettings::Create()
{
    // Load the theme for this screen
    if (!LoadWindowFromXML("musicsettings-ui.xml", "ratingsettings", this))
        return false;

    bool err = false;

    UIUtilE::Assign(this, m_ratingWeight, "ratingweight", &err);
    UIUtilE::Assign(this, m_playCountWeight, "playcountweight", &err);
    UIUtilE::Assign(this, m_lastPlayWeight, "lastplayweight", &err);
    UIUtilE::Assign(this, m_randomWeight, "randomweight", &err);
    UIUtilE::Assign(this, m_saveButton, "save", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'ratingsettings'");
        return false;
    }

    m_ratingWeight->SetRange(0,100,1);
    m_ratingWeight->SetValue(gCoreContext->GetNumSetting("IntelliRatingWeight"));
    m_playCountWeight->SetRange(0,100,1);
    m_playCountWeight->SetValue(gCoreContext->GetNumSetting("IntelliPlayCountWeight"));
    m_lastPlayWeight->SetRange(0,100,1);
    m_lastPlayWeight->SetValue(gCoreContext->GetNumSetting("IntelliLastPlayWeight"));
    m_randomWeight->SetRange(0,100,1);
    m_randomWeight->SetValue(gCoreContext->GetNumSetting("IntelliRandomWeight"));

    m_ratingWeight->SetHelpText(tr("Used in \"Smart\" Shuffle mode. "
                 "This weighting affects how much strength is "
                 "given to your rating of a given track when "
                 "ordering a group of songs."));
    m_playCountWeight->SetHelpText(tr("Used in \"Smart\" Shuffle mode. "
                 "This weighting affects how much strength is "
                 "given to how many times a given track has been "
                 "played when ordering a group of songs."));
    m_lastPlayWeight->SetHelpText(tr("Used in \"Smart\" Shuffle mode. "
                 "This weighting affects how much strength is "
                 "given to how long it has been since a given "
                 "track was played when ordering a group of songs."));
    m_randomWeight->SetHelpText(tr("Used in \"Smart\" Shuffle mode. "
                 "This weighting affects how much strength is "
                 "given to good old (pseudo-)randomness "
                 "when ordering a group of songs."));
    m_cancelButton->SetHelpText(tr("Exit without saving settings"));
    m_saveButton->SetHelpText(tr("Save settings and Exit"));

    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    BuildFocusList();

    return true;
}

void RatingSettings::slotSave(void)
{
    gCoreContext->SaveSetting("IntelliRatingWeight", m_ratingWeight->GetValue());
    gCoreContext->SaveSetting("IntelliPlayCountWeight", m_playCountWeight->GetValue());
    gCoreContext->SaveSetting("IntelliLastPlayWeight", m_lastPlayWeight->GetValue());
    gCoreContext->SaveSetting("IntelliRandomWeight", m_randomWeight->GetValue());

    gCoreContext->dispatch(MythEvent(QString("MUSIC_SETTINGS_CHANGED RATING_SETTINGS")));

    Close();
}


