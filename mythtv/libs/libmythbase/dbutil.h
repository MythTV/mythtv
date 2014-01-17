#ifndef DBUTIL_H_
#define DBUTIL_H_

#include <QStringList>

#include "mythbaseexp.h"
#include "mythdbcon.h"

enum MythDBBackupStatus
{
    kDB_Backup_Unknown = 0,
    kDB_Backup_Failed,
    kDB_Backup_Completed,
    kDB_Backup_Empty_DB,
    kDB_Backup_Disabled
};

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
class MBASE_PUBLIC DBUtil
{
  public:
    DBUtil();
    ~DBUtil() { }

    QString GetDBMSVersion(void);
    int CompareDBMSVersion(int major, int minor=0, int point=0);

    MythDBBackupStatus BackupDB(QString &filename,
                                bool disableRotation = false);
    static bool CheckTables(const bool repair = false,
                            const QString &options = "QUICK");
    static bool RepairTables(const QStringList &tables);

    static bool IsNewDatabase(void);
    static bool IsBackupInProgress(void);
    static int  CountClients(void);

    static bool TryLockSchema(MSqlQuery &, uint timeout_secs);
    static void UnlockSchema(MSqlQuery &);

    static bool CheckTimeZoneSupport(void);

    static const int kUnknownVersionNumber;

  protected:
    static bool CreateTemporaryDBConf(
        const QString &privateinfo, QString &filename);

  private:
    bool QueryDBMSVersion(void);
    bool ParseDBMSVersion(void);

    static QStringList GetTables(const QStringList &engines = QStringList());
    static QStringList CheckRepairStatus(MSqlQuery &query);

    QString CreateBackupFilename(QString prefix = "mythconverg",
                                 QString extension = ".sql");
    QString GetBackupDirectory();

    bool DoBackup(const QString &backupScript, QString &filename,
                  bool disableRotation = false);
    bool DoBackup(QString &filename);

    QString m_versionString;

    int m_versionMajor;
    int m_versionMinor;
    int m_versionPoint;

};

#endif
