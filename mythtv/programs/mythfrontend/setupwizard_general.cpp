// qt
#include <QString>
#include <QVariant>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>

#include "audiogeneralsettings.h"
#include "setupwizard_general.h"
#include "setupwizard_audio.h"

// ---------------------------------------------------

GeneralSetupWizard::GeneralSetupWizard(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_ldateButtonList(NULL),     m_sdateButtonList(NULL),
      m_timeButtonList(NULL),      m_nextButton(NULL),
      m_cancelButton(NULL)
{
}

bool GeneralSetupWizard::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "generalwizard", this);

    if (!foundtheme)
        return false;

    m_ldateButtonList = dynamic_cast<MythUIButtonList *> (GetChild("longdate"));
    m_sdateButtonList = dynamic_cast<MythUIButtonList *> (GetChild("shortdate"));
    m_timeButtonList = dynamic_cast<MythUIButtonList *> (GetChild("timeformat"));

    m_nextButton = dynamic_cast<MythUIButton *> (GetChild("next"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_ldateButtonList || !m_sdateButtonList || !m_timeButtonList ||
        !m_nextButton || !m_cancelButton)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
        return false;
    }

    m_ldateButtonList->SetHelpText( tr("Choose the date format to use when the full "
                                       "date is displayed.") );
    m_sdateButtonList->SetHelpText( tr("Choose the date format to use when a short "
                                       "date format is displayed.") );
    m_timeButtonList->SetHelpText( tr("Choose your preferred time format.") );

    m_nextButton->SetHelpText( tr("Save these changes and move on to the "
                                  "next configuration step.") );
    m_cancelButton->SetHelpText( tr("Exit this wizard, save no changes.") );

    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(slotNext()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    BuildFocusList();
    loadData();

    return true;
}

GeneralSetupWizard::~GeneralSetupWizard()
{
}

void GeneralSetupWizard::loadData()
{
    QDate sampdate = QDate::currentDate();
    QStringList ldateformats;

    ldateformats.append(sampdate.toString("ddd MMM d"));
    ldateformats.append(sampdate.toString("ddd d MMM"));
    ldateformats.append(sampdate.toString("ddd MMMM d"));
    ldateformats.append(sampdate.toString("ddd d MMMM"));
    ldateformats.append(sampdate.toString("dddd MMM d"));
    ldateformats.append(sampdate.toString("dddd d MMM"));
    ldateformats.append(sampdate.toString("MMM d"));
    ldateformats.append(sampdate.toString("d MMM"));
    ldateformats.append(sampdate.toString("MM/dd"));
    ldateformats.append(sampdate.toString("dd/MM"));
    ldateformats.append(sampdate.toString("MM.dd"));
    ldateformats.append(sampdate.toString("dd.MM"));
    ldateformats.append(sampdate.toString("M/d/yyyy"));
    ldateformats.append(sampdate.toString("d/M/yyyy"));
    ldateformats.append(sampdate.toString("MM.dd.yyyy"));
    ldateformats.append(sampdate.toString("dd.MM.yyyy"));
    ldateformats.append(sampdate.toString("yyyy-MM-dd"));
    ldateformats.append(sampdate.toString("ddd MMM d yyyy"));
    ldateformats.append(sampdate.toString("ddd d MMM yyyy"));
    ldateformats.append(sampdate.toString("ddd yyyy-MM-dd"));

    for (QStringList::const_iterator i = ldateformats.begin();
            i != ldateformats.end(); i++)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_ldateButtonList, *i);
        item->SetData(*i);
    }

    QStringList sdateformats;

    sdateformats.append(sampdate.toString("M/d"));
    sdateformats.append(sampdate.toString("d/M"));
    sdateformats.append(sampdate.toString("MM/dd"));
    sdateformats.append(sampdate.toString("dd/MM"));
    sdateformats.append(sampdate.toString("MM.dd"));
    sdateformats.append(sampdate.toString("dd.MM."));
    sdateformats.append(sampdate.toString("M.d."));
    sdateformats.append(sampdate.toString("d.M."));
    sdateformats.append(sampdate.toString("MM-dd"));
    sdateformats.append(sampdate.toString("dd-MM"));
    sdateformats.append(sampdate.toString("MMM d"));
    sdateformats.append(sampdate.toString("d MMM"));
    sdateformats.append(sampdate.toString("ddd d"));
    sdateformats.append(sampdate.toString("d ddd"));
    sdateformats.append(sampdate.toString("ddd M/d"));
    sdateformats.append(sampdate.toString("ddd d/M"));
    sdateformats.append(sampdate.toString("M/d ddd"));
    sdateformats.append(sampdate.toString("d/M ddd"));

    for (QStringList::const_iterator i = sdateformats.begin();
            i != sdateformats.end(); i++)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_sdateButtonList, *i);
        item->SetData(*i);
    }

    QStringList timeformats;
    QTime samptime = QTime::currentTime();

    timeformats.append(samptime.toString("h:mm AP"));
    timeformats.append(samptime.toString("h:mm ap"));
    timeformats.append(samptime.toString("hh:mm AP"));
    timeformats.append(samptime.toString("hh:mm ap"));
    timeformats.append(samptime.toString("h:mm"));
    timeformats.append(samptime.toString("hh:mm"));
    timeformats.append(samptime.toString("hh.mm"));

    for (QStringList::const_iterator i = timeformats.begin();
            i != timeformats.end(); i++)
    {
        MythUIButtonListItem *item =
                    new MythUIButtonListItem(m_timeButtonList, *i);
        item->SetData(*i);
    }

}
void GeneralSetupWizard::slotNext(void)
{
    save();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    AudioSetupWizard *sw = new AudioSetupWizard(mainStack, this, "audiosetupwizard");

    if (sw->Create())
    {
        mainStack->AddScreen(sw);
    }
    else
        delete sw;
}

void GeneralSetupWizard::save(void)
{
}

bool GeneralSetupWizard::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}
