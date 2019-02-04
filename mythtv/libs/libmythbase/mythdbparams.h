#ifndef MYTHDBPARAMS_H_
#define MYTHDBPARAMS_H_

#include <QString>

#include "mythbaseexp.h"

/// Structure containing the basic Database parameters
class MBASE_PUBLIC DatabaseParams
{
  public:
    DatabaseParams() {}

    void LoadDefaults(void);
    bool IsValid(const QString &source = QString("Unknown")) const;

    bool operator==(const DatabaseParams &other) const;
    bool operator!=(const DatabaseParams &other) const
        { return !((*this)==other); }

    QString dbHostName    {"localhost"};  ///< database server
    bool    dbHostPing    {true};         ///< Can we test connectivity using ping?
    int     dbPort        {3306};         ///< database port
    QString dbUserName    {"mythtv"};     ///< DB user name
    QString dbPassword    {"mythconverg"};///< DB password
    QString dbName        {"mythtv"};     ///< database name
    QString dbType        {"QMYSQL"};     ///< database type (MySQL, Postgres, etc.)

    bool    localEnabled  {false};        ///< true if localHostName is not default
    QString localHostName {"my-unique-identifier-goes-here"};
                                          ///< name used for loading/saving settings

    bool    wolEnabled    {false};        ///< true if wake-on-lan params are used
    int     wolReconnect  {0};            ///< seconds to wait for reconnect
    int     wolRetry      {5};            ///< times to retry to reconnect
    QString wolCommand    {"echo 'WOLsqlServerCommand not set'"};
                                          ///< command to use for wake-on-lan

    bool    forceSave     {false};        ///< set to true to force a save of the settings file

    QString verVersion;         ///< git version string
    QString verBranch;          ///< git branch
    QString verProtocol;        ///< backend protocol
    QString verBinary;          ///< binary library version
    QString verSchema;          ///< core schema version
};

#endif
