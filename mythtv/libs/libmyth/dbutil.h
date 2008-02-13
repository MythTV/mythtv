#ifndef DBUTIL_H_
#define DBUTIL_H_

#include <qstringlist.h>

#include "mythexp.h"

/** \class DBUtil
 *  \brief Aggregates database and DBMS utility functions.
 *
 *   This class allows retrieving or comparing the DBMS server version, and
 *   backing up the database.
 *
 *   The backup functionality currently requires mysqldump to be installed on
 *   the system.  This may change in the future to allow backups even when
 *   there is no DB client installation on the system.
 *
 *  \sa HouseKeeper::RunHouseKeeping(void)
 */
class MPUBLIC DBUtil
{
  public:
    DBUtil();
    ~DBUtil() { }

    QString GetDBMSVersion(void);
    int CompareDBMSVersion(int major, int minor=0, int point=0);

    bool BackupDB(QString &filename);

    static bool IsBackupInProgress(void);

    static const int kUnknownVersionNumber;

  private:
    bool QueryDBMSVersion(void);
    bool ParseDBMSVersion(void);

    QStringList GetTables(void);

    QString CreateBackupFilename(QString prefix = "mythconverg",
                                 QString extension = ".sql");
    QString GetBackupDirectory();

    bool DoBackup(QString &filename);

    QString m_versionString;

    int m_versionMajor;
    int m_versionMinor;
    int m_versionPoint;

};

#endif
