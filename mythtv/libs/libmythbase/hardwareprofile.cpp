#include "hardwareprofile.h"

// qt
#include <QStringList>
#include <QDir>
#include <QTextStream>

// libmythbase
#include "mythcorecontext.h"
#include "mythdirs.h"
#include "mythlogging.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"
#include "mythdate.h"

const QString SMOLT_SERVER_LOCATION =
                  QString("http://smolt.mythtv.org/");
const QString SMOLT_TOKEN =
                  QString("smolt_token-smolt.mythtv.org");

HardwareProfile::HardwareProfile() :
    m_enabled(false),
    m_uuid(QString()),               m_publicuuid(QString()),
    m_lastUpdate(QDateTime()),       m_hardwareProfile(QString())
{
    m_enabled = (gCoreContext->GetNumSetting("HardwareProfileEnabled", 0) == 1);
    m_uuid = gCoreContext->GetSetting("HardwareProfileUUID");
    m_publicuuid = gCoreContext->GetSetting("HardwareProfilePublicUUID");

    if (m_enabled)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT lastrun FROM housekeeping"
                      " WHERE      tag = 'HardwareProfiler'"
                      "   AND hostname = :HOST");
        query.bindValue(":HOST", gCoreContext->GetHostName());
        if (query.exec() && query.next())
            m_lastUpdate = MythDate::as_utc(query.value(0).toDateTime());
    }
}

HardwareProfile::~HardwareProfile()
{
}

void HardwareProfile::Enable(void)
{
    if (m_uuid.isEmpty())
        return;

    gCoreContext->SaveSettingOnHost("HardwareProfileEnabled", "1", "");
}

void HardwareProfile::Disable(void)
{
    gCoreContext->SaveSettingOnHost("HardwareProfileEnabled", "0", "");
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
        LOG(VB_GENERAL, LOG_INFO,
                 "No UUID in DB or File, generating new UUID...");

        QString cmd = GetShareDir() + "hardwareprofile/sendProfile.py";
        QStringList args;
        args << "-p";
        MythSystemLegacy system(cmd, args, kMSRunShell | kMSStdOut);

        system.Run();
        system.Wait();
        m_hardwareProfile = system.ReadAll();
        m_uuid = GetPrivateUUIDFromFile();
    }
    else if (fileUUID.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO,
                 QString("Writing Database UUID to local file: %1")
                         .arg(m_uuid));
        WritePrivateUUIDToFile(m_uuid);
    }
    else if (m_uuid.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO,
                 QString("Profile UUID found in local file: %1")
                         .arg(fileUUID));
        m_uuid = fileUUID;
    }

    // Get the Public UUID from file

    m_publicuuid = GetPublicUUIDFromFile();
}

QString HardwareProfile::GetPrivateUUIDFromFile() const
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

QString HardwareProfile::GetPublicUUIDFromFile() const
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

QString HardwareProfile::GetAdminPasswordFromFile() const
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

bool HardwareProfile::NeedsUpdate(void) const
{
    if (!m_lastUpdate.isNull() &&
        (m_lastUpdate.addMonths(1) < MythDate::current()) &&
        !m_uuid.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO,
                 "Last hardware profile update was > 30 days ago, update "
                 "required...");
        return true;
    }

    return false;
}

bool HardwareProfile::SubmitProfile(bool updateTime)
{
    if (m_uuid.isEmpty())
        return false;

    if (!m_enabled)
        Enable();

    if (!m_hardwareProfile.isEmpty())
        LOG(VB_GENERAL, LOG_INFO,
                 QString("Submitting the following hardware profile:  %1")
                         .arg(m_hardwareProfile));

    QString cmd = GetShareDir() + "hardwareprofile/sendProfile.py";
    QStringList args;
    args << "--submitOnly";
    args << "-a";
    MythSystemLegacy system(cmd, args, kMSRunShell | kMSStdOut);

    system.Run();
    if (system.Wait() == GENERIC_EXIT_OK)
    {
        GenerateUUIDs();
        gCoreContext->SaveSetting("HardwareProfileUUID", GetPrivateUUID());
        gCoreContext->SaveSetting("HardwareProfilePublicUUID", GetPublicUUID());

        if (updateTime)
        {
            HardwareProfileTask task;
            task.UpdateLastRun(MythDate::current());
        }

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

    LOG(VB_GENERAL, LOG_INFO,
             QString("Deleting the following hardware profile: %1")
                     .arg(m_uuid));

    QString cmd = GetShareDir() + "hardwareprofile/deleteProfile.py";
    QStringList args;
    MythSystemLegacy system(cmd, args, kMSRunShell | kMSStdOut);

    system.Run();
    if (system.Wait() == GENERIC_EXIT_OK)
    {
        gCoreContext->SaveSetting("HardwareProfileUUID", "");
        gCoreContext->SaveSetting("HardwareProfilePublicUUID", "");
        gCoreContext->SaveSetting("HardwareProfileLastUpdated",
                                  MythDate::current_iso_string());
        return true;
    }
    else
        return false;

    return false;
}

QString HardwareProfile::GetProfileURL() const
{
    QString ret;

    if (!gCoreContext->GetSetting("HardwareProfileUUID").isEmpty())
    {
        ret = SMOLT_SERVER_LOCATION + "client/show/?uuid=" + m_publicuuid;
    }

    return ret;
}

QString HardwareProfile::GetHardwareProfile() const
{
    QString cmd = GetShareDir() + "hardwareprofile/sendProfile.py";
    QStringList args;
    args << "-p";
    MythSystemLegacy system(cmd, args, kMSRunShell | kMSStdOut);

    system.Run();
    system.Wait();
    return system.ReadAll();
}

bool HardwareProfileTask::DoCheckRun(QDateTime now)
{
    if (gCoreContext->GetNumSetting("HardwareProfileEnabled", 0) == 0)
        // global disable, we don't want to run
        return false;

    // leave it up to the standard periodic calculation, 30 +-1 days
    return PeriodicHouseKeeperTask::DoCheckRun(now);
}

bool HardwareProfileTask::DoRun(void)
{
    HardwareProfile hp;
    return hp.SubmitProfile(false);
}
