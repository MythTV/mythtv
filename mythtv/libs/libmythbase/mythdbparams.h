#ifndef MYTHDBPARAMS_H_
#define MYTHDBPARAMS_H_

#include <QString>

#include "mythbaseexp.h"

/// Structure containing the basic Database parameters
class MBASE_PUBLIC DatabaseParams
{
  public:
    DatabaseParams() { LoadDefaults(); }

    void LoadDefaults(void);
    bool IsValid(const QString &source = QString("Unknown")) const;

    bool operator==(const DatabaseParams &other) const;
    bool operator!=(const DatabaseParams &other) const
        { return !((*this)==other); }

    QString dbHostName;         ///< database server
    bool    dbHostPing;         ///< Can we test connectivity using ping?
    int     dbPort;             ///< database port
    QString dbUserName;         ///< DB user name
    QString dbPassword;         ///< DB password
    QString dbName;             ///< database name
    QString dbType;             ///< database type (MySQL, Postgres, etc.)

    bool    localEnabled;       ///< true if localHostName is not default
    QString localHostName;      ///< name used for loading/saving settings

    bool    wolEnabled;         ///< true if wake-on-lan params are used
    int     wolReconnect;       ///< seconds to wait for reconnect
    int     wolRetry;           ///< times to retry to reconnect
    QString wolCommand;         ///< command to use for wake-on-lan

    bool    forceSave;          ///< set to true to force a save of the settings file

    QString verVersion;         ///< git version string
    QString verBranch;          ///< git branch
    QString verProtocol;        ///< backend protocol
    QString verBinary;          ///< binary library version
    QString verSchema;          ///< core schema version
};

#endif

