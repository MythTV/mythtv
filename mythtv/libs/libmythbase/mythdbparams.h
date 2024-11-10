#ifndef MYTHDBPARAMS_H_
#define MYTHDBPARAMS_H_

#include <QString>

#include "mythbaseexp.h"
#include "mythchrono.h"

/// Structure containing the basic Database parameters
class MBASE_PUBLIC DatabaseParams
{
  public:
    DatabaseParams() = default;

    void LoadDefaults(void);
    bool IsValid(const QString &source = QString("Unknown")) const;

    bool operator==(const DatabaseParams &other) const;
    bool operator!=(const DatabaseParams &other) const
        { return !((*this)==other); }

    QString m_dbHostName    {"localhost"};  ///< database server
    bool    m_dbHostPing    {false};        ///< No longer used
    int     m_dbPort        {3306};         ///< database port
    QString m_dbUserName    {"mythtv"};     ///< DB user name
    QString m_dbPassword    {"mythtv"};     ///< DB password
    QString m_dbName        {"mythconverg"};     ///< database name
    QString m_dbType        {"QMYSQL"};     ///< database type (MySQL, Postgres, etc.)

    bool    m_localEnabled  {false};        ///< true if localHostName is not default
    QString m_localHostName {"my-unique-identifier-goes-here"};
                                          ///< name used for loading/saving settings

    bool    m_wolEnabled    {false};        ///< true if wake-on-lan params are used
    std::chrono::seconds m_wolReconnect  {0s}; ///< seconds to wait for reconnect
    int     m_wolRetry      {5};            ///< times to retry to reconnect
    QString m_wolCommand    {"echo 'WOLsqlServerCommand not set'"};
                                          ///< command to use for wake-on-lan

    bool    m_forceSave     {false};        ///< set to true to force a save of the settings file

    QString m_verVersion;         ///< git version string
    QString m_verBranch;          ///< git branch
    QString m_verProtocol;        ///< backend protocol
    QString m_verBinary;          ///< binary library version
    QString m_verSchema;          ///< core schema version
};

#endif
