#include "hardwareprofile.h"

// qt
#include <QStringList>
#include <QDir>

// libmythbase
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "mythverbose.h"
#include "mythsystem.h"
#include "exitcodes.h"

// libmythui
#include "mythmainwindow.h"
#include "mythdialogbox.h"
#include "mythprogressdialog.h"

const QString SMOLT_SERVER_LOCATION =
                  QString("http://smolt.mythvantage.com/");

HardwareProfile::HardwareProfile() :
    m_popupStack(NULL),              m_busyPopup(NULL),
    m_uuid(QString()),               m_publicuuid(QString()),
    m_hardwareProfile(QString())
{
    m_popupStack = GetMythMainWindow()->GetStack("popup stack");
    m_uuid = gCoreContext->GetSetting("HardwareProfileUUID");
    m_publicuuid = gCoreContext->GetSetting("HardwareProfilePublicUUID");
}

HardwareProfile::~HardwareProfile()
{
}

bool HardwareProfile::Prompt(bool force)
{
    if (m_uuid.isEmpty() || force)
        return true;
    else
        return false;
}

void HardwareProfile::ShowPrompt()
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
            SLOT(OnPromptReturn(bool)));
}

void HardwareProfile::OnPromptReturn(bool submit)
{
    if (submit)
    {
        CreateBusyDialog(tr("Submitting your hardware profile..."));
        GenerateUUIDs();
        if (SubmitProfile())
        {
            if (m_busyPopup)
            {
                m_busyPopup->Close();
                m_busyPopup = NULL;
            }
            gCoreContext->SaveSetting("HardwareProfileUUID", m_uuid);
            gCoreContext->SaveSetting("HardwareProfilePublicUUID", m_publicuuid);
    VERBOSE(VB_GENERAL, QString("Profile URL: %1")
                            .arg(GetProfileURL()));
            ShowOkPopup(tr("Hardware profile submitted. Thank you for supporting "
                           "MythTV!"));
        }
        else
            ShowOkPopup(tr("Encountered a problem while submitting your profile."));
    }
    else
    {
        gCoreContext->SaveSetting("HardwareProfileUUID", "-1");
        gCoreContext->SaveSetting("HardwareProfilePublicUUID", "-1");
    }
}

void HardwareProfile::GenerateUUIDs(void)
{
    QString fileprefix = GetConfDir() + "/HardwareProfile";
    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    // Generate the Private Hardware UUID (or recover them from the DB or file)

    QString fileUUID = GetPrivateUUIDFromFile();

    if (fileUUID.isEmpty() && m_uuid.isEmpty())
    {
        VERBOSE(VB_GENERAL, QString("No UUID in DB or File, generating new UUID..."));

        QString cmd = GetShareDir() + "hardwareprofile/sendProfile.py";
        QStringList args;
        args << "-p";
        MythSystem system(cmd, args, kMSRunShell | kMSStdOut | kMSBuffered);

        system.Run();
        system.Wait();
        m_hardwareProfile = system.ReadAll();
        m_uuid = GetPrivateUUIDFromFile();
    }
    else if (fileUUID.isEmpty())
    {
        VERBOSE(VB_GENERAL, QString("Writing Database UUID to local file: %1")
                        .arg(m_uuid));
        WritePrivateUUIDToFile(m_uuid);
    }
    else if (m_uuid.isEmpty())
    {
        VERBOSE(VB_GENERAL, QString("Profile UUID found in local file: %1")
                           .arg(fileUUID));
        m_uuid = fileUUID;
    }

    // Get the Public UUID from file if necessary

    if (m_publicuuid.isEmpty())
        m_publicuuid = GetPublicUUIDFromFile();

    if (m_busyPopup)
    {
        m_busyPopup->Close();
        m_busyPopup = NULL;
    }
}

QString HardwareProfile::GetPrivateUUIDFromFile()
{
    QString ret;

    QString hwuuid_file = GetConfDir() + "/HardwareProfile/hw-uuid";
    QFile file(hwuuid_file);
    if (file.open( QIODevice::ReadOnly ))
    {
        QTextStream stream(&file);
        ret = stream.readLine();
        file.close();
    }

    return ret;
}

QString HardwareProfile::GetPublicUUIDFromFile()
{
    QString ret;

    QString pubuuid_file = GetConfDir() + "/HardwareProfile/uuiddb.cfg";
    QString pubuuid;
    QFile pubfile(pubuuid_file);
    if (pubfile.open( QIODevice::ReadOnly ))
    {
        QString s;
        QTextStream stream(&pubfile);
        while ( !stream.atEnd() )
        {
            s = stream.readLine();
            if (s.contains(m_uuid))
            {
                ret = s.section("=",1,1);
                ret = ret.trimmed();
            }
        }
        pubfile.close();
    }

    return ret;
}

bool HardwareProfile::WritePrivateUUIDToFile(QString uuid)
{
    QString hwuuid_file = GetConfDir() + "/HardwareProfile/hw-uuid";
    QFile file(hwuuid_file);
    if (file.open(QIODevice::WriteOnly))
    {
        QTextStream stream(&file);
        stream << uuid;
        file.close();
        return true;
    }
    else
        return false;
}

bool HardwareProfile::SubmitProfile(void)
{
    if (m_uuid.isEmpty())
        return false;

    if (!m_hardwareProfile.isEmpty())
        VERBOSE(VB_GENERAL, QString("Submitting the following hardware profile:\n%1")
                            .arg(m_hardwareProfile));

    QString cmd = GetShareDir() + "hardwareprofile/sendProfile.py";
    QStringList args;
    args << "--submitOnly";
    args << "-a";
    MythSystem system(cmd, args, kMSRunShell | kMSStdOut | kMSBuffered);

    system.Run();
    if (system.Wait() == GENERIC_EXIT_OK)
        return true;
    else
        return false;

    return false;
}

bool HardwareProfile::DeleteProfile(void)
{
    if (m_uuid.isEmpty())
        return false;

    VERBOSE(VB_GENERAL, QString("Deleting the following hardware profile: %1")
                            .arg(m_uuid));

    QString cmd = GetShareDir() + "hardwareprofile/deleteProfile.py";
    QStringList args;
    MythSystem system(cmd, args, kMSRunShell | kMSStdOut | kMSBuffered);

    system.Run();
    if (system.Wait() == GENERIC_EXIT_OK)
        return true;
    else
        return false;

    return false;
}

QString HardwareProfile::GetProfileURL()
{
    QString ret;

    if (!m_publicuuid.isEmpty())
    {
        ret = SMOLT_SERVER_LOCATION + "client/show/?uuid=" + m_publicuuid;
    }
    else
        ret = tr("No Profile Exists");

    return ret;
}

void HardwareProfile::CreateBusyDialog(QString message)
{
    if (m_busyPopup)
        return;

    m_busyPopup = new MythUIBusyDialog(message, m_popupStack,
            "hardwareprofilebusydialog");

    if (m_busyPopup->Create())
        m_popupStack->AddScreen(m_busyPopup);
}

