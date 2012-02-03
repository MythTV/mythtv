#include "mythdbparams.h"
#include "mythlogging.h"

/// Load sensible connection defaults.
void DatabaseParams::LoadDefaults(void)
{
    dbHostName    = "localhost";
    dbHostPing    = true;
    dbPort        = 3306;
    dbUserName    = "mythtv";
    dbPassword    = "mythtv";
    dbName        = "mythconverg";
    dbType        = "QMYSQL";

    localEnabled  = false;
    localHostName = "my-unique-identifier-goes-here";

    wolEnabled    = false;
    wolReconnect  = 0;
    wolRetry      = 5;
    wolCommand    = "echo 'WOLsqlServerCommand not set'";

    forceSave     = false;

    verVersion.clear();
    verBranch.clear();
    verProtocol.clear();
    verBinary.clear();
    verSchema.clear();
}

bool DatabaseParams::IsValid(const QString &source) const
{
    // Print some warnings if things look fishy..
    QString msg = QString(" is not set in %1").arg(source);

    if (dbHostName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBHostName" + msg);
        return false;
    }
    if (dbUserName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBUserName" + msg);
        return false;
    }
    if (dbPassword.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBPassword" + msg);
        return false;
    }
    if (dbName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "DBName" + msg);
        return false;
    }

    return true;
}

bool DatabaseParams::operator==(const DatabaseParams &other) const
{
    return
        dbHostName   == other.dbHostName     &&
        dbHostPing   == other.dbHostPing     &&
        dbPort       == other.dbPort         &&
        dbUserName   == other.dbUserName     &&
        dbPassword   == other.dbPassword     &&
        dbName       == other.dbName         &&
        dbType       == other.dbType         &&
        localEnabled == other.localEnabled   &&
        wolEnabled   == other.wolEnabled     &&
        (!localEnabled || (localHostName == other.localHostName)) &&
        (!wolEnabled ||
         (wolReconnect == other.wolReconnect &&
          wolRetry     == other.wolRetry     &&
          wolCommand   == other.wolCommand));
}
