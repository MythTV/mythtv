#ifndef MYTHDBPARAMS_H_
#define MYTHDBPARAMS_H_

#include "mythbaseexp.h"

/// Structure containing the basic Database parameters
struct MBASE_PUBLIC DatabaseParams
{
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
};

#endif

