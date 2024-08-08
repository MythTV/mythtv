// Qt
#include <QString>
#include <QVariant>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/hardwareprofile.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythsystemlegacy.h"

// MythFrontend
#include "audiogeneralsettings.h"
#include "setupwizard_general.h"
#include "setupwizard_audio.h"

// ---------------------------------------------------

GeneralSetupWizard::GeneralSetupWizard(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_hardwareProfile(new HardwareProfile())
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
}

bool GeneralSetupWizard::Create()
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("config-ui.xml", "generalwizard", this);
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

    connect(m_submitButton, &MythUIButton::Clicked, this, &GeneralSetupWizard::slotSubmit);
    connect(m_viewButton, &MythUIButton::Clicked, this, &GeneralSetupWizard::slotView);
    connect(m_deleteButton, &MythUIButton::Clicked, this, &GeneralSetupWizard::slotDelete);

    connect(m_nextButton, &MythUIButton::Clicked, this, &GeneralSetupWizard::slotNext);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &MythScreenType::Close);

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

void GeneralSetupWizard::loadData()
{
    if (!m_hardwareProfile)
        return;

    m_hardwareProfile->GenerateUUIDs();

    if (m_profileLocation)
        m_profileLocation->SetText(m_hardwareProfile->GetProfileURL());

    if (m_adminPassword)
        m_adminPassword->SetText(HardwareProfile::GetAdminPasswordFromFile());
}

void GeneralSetupWizard::slotNext(void)
{
    save();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *sw = new AudioSetupWizard(mainStack, this, "audiosetupwizard");

    if (sw->Create())
    {
        mainStack->AddScreen(sw);
    }
    else
    {
        delete sw;
    }
}

void GeneralSetupWizard::slotSubmit(void)
{
    QString message = tr("Would you like to share your "
                         "hardware profile with the MythTV developers? "
                         "Profiles are anonymous and are a great way to "
                         "help with future development.");
    auto *confirmdialog = new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
        m_popupStack->AddScreen(confirmdialog);

    connect(confirmdialog, &MythConfirmationDialog::haveResult,
            this, &GeneralSetupWizard::OnSubmitPromptReturn);
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
                m_busyPopup = nullptr;
            }
            ShowOkPopup(tr("Hardware profile submitted. Thank you for supporting "
                           "MythTV!"));
            if (m_profileLocation)
                m_profileLocation->SetText(m_hardwareProfile->GetProfileURL());
            if (m_adminPassword)
                m_adminPassword->SetText(HardwareProfile::GetAdminPasswordFromFile());
        }
        else
        {
            if (m_busyPopup)
            {
                m_busyPopup->Close();
                m_busyPopup = nullptr;
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

    QString cmd = browser;
    cmd.replace("%ZOOM%", zoom);
    cmd.replace("%URL%", url);
    cmd.replace('\'', "%27");
    cmd.replace("&","\\&");
    cmd.replace(";","\\;");

    GetMythMainWindow()->AllowInput(false);
    myth_system(cmd, kMSDontDisableDrawing);
    GetMythMainWindow()->AllowInput(true);
}

void GeneralSetupWizard::slotDelete(void)
{
    if (gCoreContext->GetSetting("HardwareProfileUUID").isEmpty())
    {
        ShowOkPopup(tr("You haven't submitted your hardware profile yet!"));
        return;
    }

    QString message = tr("Are you sure you want to delete "
                         "your online profile?  Your information "
                         "is anonymous and helps the developers "
                         "to know what hardware the majority of users "
                         "prefer.");
    auto *confirmdialog = new MythConfirmationDialog(m_popupStack,message);

    if (confirmdialog->Create())
        m_popupStack->AddScreen(confirmdialog);

    connect(confirmdialog, &MythConfirmationDialog::haveResult,
            this, &GeneralSetupWizard::OnDeletePromptReturn);
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
                m_busyPopup = nullptr;
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
                m_busyPopup = nullptr;
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

    if (MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void GeneralSetupWizard::CreateBusyDialog(const QString& message)
{
    if (m_busyPopup)
        return;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "setupwizardbusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
}
