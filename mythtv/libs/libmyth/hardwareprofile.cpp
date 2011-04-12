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
                  QString("http://smolt.mythtv.org/");
const QString SMOLT_TOKEN =
                  QString("smolt_token-smolt.mythtv.org");

HardwareProfile::HardwareProfile() :
    m_uuid(QString()),               m_publicuuid(QString()),
    m_lastUpdate(QDateTime()),       m_hardwareProfile(QString())
{
    m_uuid = gCoreContext->GetSetting("HardwareProfileUUID");
    m_publicuuid = gCoreContext->GetSetting("HardwareProfilePublicUUID");
    QString lastupdate = gCoreContext->GetSetting("HardwareProfileLastUpdated");
    m_lastUpdate = QDateTime::fromString(lastupdate, Qt::ISODate);
}

HardwareProfile::~HardwareProfile()
{
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

    // Get the Public UUID from file

    m_publicuuid = GetPublicUUIDFromFile();
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

QString HardwareProfile::GetAdminPasswordFromFile()
{
    QString ret;

    if (gCoreContext->GetSetting("HardwareProfileUUID").isEmpty())
        return ret;

    QString token_file = GetConfDir() + "/HardwareProfile/" + SMOLT_TOKEN;
    QFile file(token_file);
    if (file.open( QIODevice::ReadOnly ))
    {
        QTextStream stream(&file);
        ret = stream.readLine();
        file.close();
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

bool HardwareProfile::NeedsUpdate(void)
{
    if (!m_lastUpdate.isNull() &&
        (m_lastUpdate.addMonths(1) < QDateTime::currentDateTime()) &&
        !m_uuid.isEmpty())
    {
        VERBOSE(VB_GENERAL, QString("Last hardware profile update was > 30 days ago, update required..."));
        return true;
    }

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
    {
        GenerateUUIDs();
        gCoreContext->SaveSetting("HardwareProfileUUID", GetPrivateUUID());
        gCoreContext->SaveSetting("HardwareProfilePublicUUID", GetPublicUUID());
        gCoreContext->SaveSetting("HardwareProfileLastUpdated",
                                  QDateTime::currentDateTime().toString(Qt::ISODate));
        return true;
    }
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
    {
        gCoreContext->SaveSetting("HardwareProfileUUID", "");
        gCoreContext->SaveSetting("HardwareProfilePublicUUID", "");
        gCoreContext->SaveSetting("HardwareProfileLastUpdated",
                                  QDateTime::currentDateTime().toString(Qt::ISODate));
        return true;
    }
    else
        return false;

    return false;
}

QString HardwareProfile::GetProfileURL()
{
    QString ret;

    if (!gCoreContext->GetSetting("HardwareProfileUUID").isEmpty())
    {
        ret = SMOLT_SERVER_LOCATION + "client/show/?uuid=" + m_publicuuid;
    }

    return ret;
}

QString HardwareProfile::GetHardwareProfile()
{
    QString cmd = GetShareDir() + "hardwareprofile/sendProfile.py";
    QStringList args;
    args << "-p";
    MythSystem system(cmd, args, kMSRunShell | kMSStdOut | kMSBuffered);

    system.Run();
    system.Wait();
    return system.ReadAll();
}
