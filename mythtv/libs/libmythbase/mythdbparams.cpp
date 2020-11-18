#include "mythdbparams.h"
#include "mythlogging.h"

/// Load sensible connection defaults.
///
/// This duplicates the initializer information, but is needed so that
/// an existing object can be reset to the default values instead of
/// creating a new object.
void DatabaseParams::LoadDefaults(void)
{
    m_dbHostName    = "localhost";
    m_dbHostPing    = true;
    m_dbPort        = 3306;
    m_dbUserName    = "mythtv";
    m_dbPassword    = "mythtv";
    m_dbName        = "mythconverg";
    m_dbType        = "QMYSQL";

    m_localEnabled  = false;
    m_localHostName = "my-unique-identifier-goes-here";

    m_wolEnabled    = false;
    m_wolReconnect  = 0s;
    m_wolRetry      = 5;
    m_wolCommand    = "echo 'WOLsqlServerCommand not set'";

    m_forceSave     = false;

    m_verVersion.clear();
    m_verBranch.clear();
    m_verProtocol.clear();
    m_verBinary.clear();
    m_verSchema.clear();
}

bool DatabaseParams::IsValid(const QString &source) const
{
    // Print some warnings if things look fishy..
    QString msg = QString(" is not set in %1").arg(source);

    if (m_dbHostName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBHostName" + msg);
        return false;
    }
    if (m_dbUserName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBUserName" + msg);
        return false;
    }
    if (m_dbPassword.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBPassword" + msg);
        return false;
    }
    if (m_dbName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBName" + msg);
        return false;
    }

    return true;
}

bool DatabaseParams::operator==(const DatabaseParams &other) const
{
    return
        m_dbHostName   == other.m_dbHostName     &&
        m_dbHostPing   == other.m_dbHostPing     &&
        m_dbPort       == other.m_dbPort         &&
        m_dbUserName   == other.m_dbUserName     &&
        m_dbPassword   == other.m_dbPassword     &&
        m_dbName       == other.m_dbName         &&
        m_dbType       == other.m_dbType         &&
        m_localEnabled == other.m_localEnabled   &&
        m_wolEnabled   == other.m_wolEnabled     &&
        (!m_localEnabled || (m_localHostName == other.m_localHostName)) &&
        (!m_wolEnabled ||
         (m_wolReconnect == other.m_wolReconnect &&
          m_wolRetry     == other.m_wolRetry     &&
          m_wolCommand   == other.m_wolCommand));
}
