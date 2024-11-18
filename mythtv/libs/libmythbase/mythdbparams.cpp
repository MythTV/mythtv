#include "mythdbparams.h"
#include "mythlogging.h"

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
