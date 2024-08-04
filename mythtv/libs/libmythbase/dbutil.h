#ifndef DBUTIL_H_
#define DBUTIL_H_

#include <QStringList>

#include "mythbaseexp.h"
#include "mythdbcon.h"

enum MythDBBackupStatus : std::uint8_t
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
    /** \fn DBUtil::DBUtil(void)
     *  \brief Constructs the DBUtil object.
     */
    DBUtil() = default;
    ~DBUtil() = default;

    QString GetDBMSVersion(void);
    int CompareDBMSVersion(int major, int minor=0, int point=0);

    static MythDBBackupStatus BackupDB(QString &filename,
                                       bool disableRotation = false);
    static bool CheckTables(bool repair = false,
                            const QString &options = "QUICK");
    static bool RepairTables(const QStringList &tables);

    static bool IsNewDatabase(void);
    static bool IsBackupInProgress(void);
    static int  CountClients(void);

    static bool TryLockSchema(MSqlQuery &query, uint timeout_secs);
    static void UnlockSchema(MSqlQuery &query);

    static bool CheckTimeZoneSupport(void);
    static bool CheckTableColumnExists(const QString &tableName, const QString &columnName);

    static const int kUnknownVersionNumber;

  protected:
    static bool CreateTemporaryDBConf(
        const QString &privateinfo, QString &filename);

  private:
    bool QueryDBMSVersion(void);
    bool ParseDBMSVersion(void);

    static QStringList GetTables(const QStringList &engines = QStringList());
    static QStringList CheckRepairStatus(MSqlQuery &query);

    static QString CreateBackupFilename(const QString& prefix = "mythconverg",
                                 const QString& extension = ".sql");
    static QString GetBackupDirectory();

    static bool DoBackup(const QString &backupScript, QString &filename,
                         bool disableRotation = false);
    static bool DoBackup(QString &filename);

    QString m_versionString;

    int m_versionMajor { -1 };
    int m_versionMinor { -1 };
    int m_versionPoint { -1 };

};

#endif
