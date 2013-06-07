// qt
#include <QString>
#include <QVariant>

// myth
#include "mythcontext.h"
#include "mythsystemlegacy.h"
#include "mythdbcon.h"
#include "mythdirs.h"

#include "hardwareprofile.h"

#include "audiogeneralsettings.h"
#include "setupwizard_general.h"
#include "setupwizard_audio.h"

// ---------------------------------------------------

GeneralSetupWizard::GeneralSetupWizard(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_submitButton(NULL),    m_viewButton(NULL),
      m_deleteButton(NULL),    m_nextButton(NULL),
      m_cancelButton(NULL),    m_profileLocation(NULL),
      m_adminPassword(NULL),   m_busyPopup(NULL)
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_hardwareProfile = new HardwareProfile();
}

bool GeneralSetupWizard::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "generalwizard", this);

    if (!foundtheme)
        return false;

    m_submitButton = dynamic_cast<MythUIButton *> (GetChild("submit"));
    m_viewButton = dynamic_cast<MythUIButton *> (GetChild("view"));
    m_deleteButton = dynamic_cast<MythUIButton *> (GetChild("delete"));

    m_nextButton = dynamic_cast<MythUIButton *> (GetChild("next"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    m_profileLocation = dynamic_cast<MythUIText *> (GetChild("profiletext"));
    m_adminPassword = dynamic_cast<MythUIText *> (GetChild("profilepassword"));

    if (!m_submitButton || !m_viewButton || !m_deleteButton ||
        !m_nextButton || !m_cancelButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    m_submitButton->SetHelpText( tr("Anonymously submit a profile of your hardware. "
                                    "This helps the developers to determine where "
                                    "to focus their efforts.") );
    m_viewButton->SetHelpText( tr("Visit your online hardware profile. (This requires "
                                  "that you have the MythBrowser plugin installed)") );
    m_deleteButton->SetHelpText( tr("Delete your online hardware profile.") );

    m_nextButton->SetHelpText( tr("Save these changes and move on to the "
                                  "next configuration step.") );
    m_cancelButton->SetHelpText( tr("Exit this wizard, save no changes.") );

    connect(m_submitButton, SIGNAL(Clicked()), this, SLOT(slotSubmit()));
    connect(m_viewButton, SIGNAL(Clicked()), this, SLOT(slotView()));
    connect(m_deleteButton, SIGNAL(Clicked()), this, SLOT(slotDelete()));

    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(slotNext()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    BuildFocusList();
    loadData();

#ifndef __linux__
#ifndef CONFIG_BINDINGS_PYTHON
    // The hardware profiler only works on linux.
    // Make the widgets invisible on other platforms.

    m_submitButton->Hide();
    m_viewButton->Hide();
    m_deleteButton->Hide();

    if (m_profileLocation)
        m_profileLocation->Hide();

    if (m_adminPassword)
        m_adminPassword->Hide();
#endif
#endif

    return true;
}

GeneralSetupWizard::~GeneralSetupWizard()
{
}

void GeneralSetupWizard::loadData()
{
    if (!m_hardwareProfile)
        return;

    m_hardwareProfile->GenerateUUIDs();

    if (m_profileLocation)
        m_profileLocation->SetText(m_hardwareProfile->GetProfileURL());

    if (m_adminPassword)
        m_adminPassword->SetText(m_hardwareProfile->GetAdminPasswordFromFile());
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

void GeneralSetupWizard::slotSubmit(void)
{
    QString message = QObject::tr("Would you like to share your "
                          "hardware profile with the MythTV developers? "
                          "Profiles are anonymous and are a great way to "
                          "help with future development.");
    MythConfirmationDialog *confirmdialog =
            new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
        m_popupStack->AddScreen(confirmdialog);

    connect(confirmdialog, SIGNAL(haveResult(bool)),
            SLOT(OnSubmitPromptReturn(bool)));
}

void GeneralSetupWizard::OnSubmitPromptReturn(bool submit)
{
    if (submit)
    {
        CreateBusyDialog(tr("Submitting your hardware profile..."));
        if (m_hardwareProfile->SubmitProfile())
        {
            if (m_busyPopup)
            {
                m_busyPopup->Close();
                m_busyPopup = NULL;
            }
            ShowOkPopup(tr("Hardware profile submitted. Thank you for supporting "
                           "MythTV!"));
            if (m_profileLocation)
                m_profileLocation->SetText(m_hardwareProfile->GetProfileURL());
            if (m_adminPassword)
                m_adminPassword->SetText(m_hardwareProfile->GetAdminPasswordFromFile());
        }
        else
        {
            if (m_busyPopup)
            {
                m_busyPopup->Close();
                m_busyPopup = NULL;
            }
            ShowOkPopup(tr("Encountered a problem while submitting your profile."));
        }
    }
}

void GeneralSetupWizard::slotView(void)
{
    if (gCoreContext->GetSetting("HardwareProfilePublicUUID").isEmpty())
    {
        ShowOkPopup(tr("You haven't submitted your hardware profile yet! "
                       "Please submit your profile to visit it online."));
        return;
    }

    QString url = m_hardwareProfile->GetProfileURL();

    LOG(VB_GENERAL, LOG_DEBUG, QString("Profile URL = %1").arg(url));

    if (url.isEmpty())
        return;

    QString browser = gCoreContext->GetSetting("WebBrowserCommand", "");
    QString zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0");

    if (browser.isEmpty())
    {
        ShowOkPopup(tr("No browser command set! MythTV needs a browser "
                       "installed and configure to display your hardware "
                       "profile."));
        return;
    }

    if (browser.toLower() == "internal")
    {
        GetMythMainWindow()->HandleMedia("WebBrowser", url);
        return;
    }
    else
    {
        QString cmd = browser;
        cmd.replace("%ZOOM%", zoom);
        cmd.replace("%URL%", url);
        cmd.replace('\'', "%27");
        cmd.replace("&","\\&");
        cmd.replace(";","\\;");

        GetMythMainWindow()->AllowInput(false);
        myth_system(cmd, kMSDontDisableDrawing);
        GetMythMainWindow()->AllowInput(true);
        return;
    }
}

void GeneralSetupWizard::slotDelete(void)
{
    if (gCoreContext->GetSetting("HardwareProfileUUID").isEmpty())
    {
        ShowOkPopup(tr("You haven't submitted your hardware profile yet!"));
        return;
    }

    QString message = QObject::tr("Are you sure you want to delete "
                                  "your online profile?  Your information "
                                  "is anonymous and helps the developers "
                                  "to know what hardware the majority of users "
                                  "prefer.");
    MythConfirmationDialog *confirmdialog =
            new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
        m_popupStack->AddScreen(confirmdialog);

    connect(confirmdialog, SIGNAL(haveResult(bool)),
            SLOT(OnDeletePromptReturn(bool)));
}

void GeneralSetupWizard::OnDeletePromptReturn(bool submit)
{
    if (submit)
    {
        CreateBusyDialog(tr("Deleting your hardware profile..."));
        if (m_hardwareProfile->DeleteProfile())
        {
            if (m_busyPopup)
            {
                m_busyPopup->Close();
                m_busyPopup = NULL;
            }
            ShowOkPopup(tr("Hardware profile deleted."));
            if (m_profileLocation)
                m_profileLocation->SetText("");
            if (m_adminPassword)
                m_adminPassword->SetText("");
        }
        else
        {
            if (m_busyPopup)
            {
                m_busyPopup->Close();
                m_busyPopup = NULL;
            }
            ShowOkPopup(tr("Encountered a problem while deleting your profile."));
        }
    }
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

void GeneralSetupWizard::CreateBusyDialog(QString message)
{
    if (m_busyPopup)
        return;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "setupwizardbusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
}

