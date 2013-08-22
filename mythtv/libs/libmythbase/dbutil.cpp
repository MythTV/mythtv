#include <climits>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QDateTime>
#include <QSqlError>
#include <QSqlRecord>

#include "dbutil.h"
#include "mythcorecontext.h"
#include "storagegroup.h"
#include "mythmiscutil.h"
#include "mythdate.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "mythlogging.h"
#include "mythsystemlegacy.h"
#include "exitcodes.h"

#define LOC QString("DBUtil: ")

const int DBUtil::kUnknownVersionNumber = INT_MIN;

/** \fn DBUtil::DBUtil(void)
 *  \brief Constructs the DBUtil object.
 */
DBUtil::DBUtil(void)
    : m_versionString(QString::null), m_versionMajor(-1), m_versionMinor(-1),
      m_versionPoint(-1)
{
}

/** \fn DBUtil::GetDBMSVersion(void)
 *  \brief Returns the QString version name of the DBMS or QString::null in
 *         the event of an error.
 */
QString DBUtil::GetDBMSVersion(void)
{
    if (m_versionString.isEmpty())
        QueryDBMSVersion();
    return m_versionString;
}

/** \fn DBUtil::CompareDBMSVersion(int, int, int)
 *  \brief Compares the version of the active DBMS with the provided version.
 *
 *   Returns negative, 0, or positive if the active DBMS version is less than,
 *   equal to, or greater than the provided version or returns
 *   DBUtil::kUnknownVersionNumber if the version cannot be determined.
 *
 *  \param major The major version number (i.e. 5 in "5.0.22")
 *  \param minor The minor version number (i.e. 0 in "5.0.22")
 *  \param point The point version number (i.e. 22 in "5.0.22")
 *  \return negative, 0, or positive or DBUtil::kUnknownVersionNumber for error
 */
int DBUtil::CompareDBMSVersion(int major, int minor, int point)
{
    if (m_versionMajor < 0)
        if (!ParseDBMSVersion())
           return kUnknownVersionNumber;

    int result = 0;
    int version[3] = {m_versionMajor, m_versionMinor, m_versionPoint};
    int compareto[3] = {major, minor, point};
    for (int i = 0; i < 3 && !result; i++)
    {
        if ((version[i] > -1) || (compareto[i] != 0))
            result = version[i] - compareto[i];
    }

    return result;
}

/** \fn DBUtil::IsNewDatabase(void)
 *  \brief Returns true for a new (empty) database.
 */
bool DBUtil::IsNewDatabase(void)
{
    const QStringList tables = GetTables();
    const int size = tables.size();
    // Usually there will be a single table called schemalock, but check for
    // no tables, also, just in case.
    return (((size == 1) && (tables.at(0).endsWith(".`schemalock`"))) ||
            (size == 0));
}

/** \fn DBUtil::IsBackupInProgress(void)
 *  \brief Test to see if a DB backup is in progress
 *
 */
bool DBUtil::IsBackupInProgress(void)
{
    QString backupStartTimeStr =
        gCoreContext->GetSetting("BackupDBLastRunStart");
    QString backupEndTimeStr = gCoreContext->GetSetting("BackupDBLastRunEnd");

    if (backupStartTimeStr.isEmpty())
    {
        LOG(VB_DATABASE, LOG_ERR, "DBUtil::BackupInProgress(): No start time "
                                  "found, database backup is not in progress.");
        return false;
    }

    backupStartTimeStr.replace(" ", "T");

    QDateTime backupStartTime = MythDate::fromString(backupStartTimeStr);

    // No end time set
    if (backupEndTimeStr.isEmpty())
    {
        // If DB Backup started less then 10 minutes ago, assume still running
        if (backupStartTime.secsTo(MythDate::current()) < 600)
        {
            LOG(VB_DATABASE, LOG_INFO,
                QString("DBUtil::BackupInProgress(): Found "
                    "database backup start time of %1 which was %2 seconds "
                    "ago, therefore it appears the backup is still running.")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(MythDate::current())));
            return true;
        }
        else
        {
            LOG(VB_DATABASE, LOG_ERR, QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, but no end time was found. "
                    "The backup started %2 seconds ago and should have "
                    "finished by now therefore it appears it is not running .")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(MythDate::current())));
            return false;
        }
    }
    else
    {
        backupEndTimeStr.replace(" ", "T");

        QDateTime backupEndTime = MythDate::fromString(backupEndTimeStr);

        if (backupEndTime >= backupStartTime)
        {
            LOG(VB_DATABASE, LOG_ERR,
                QString("DBUtil::BackupInProgress(): Found "
                    "database backup end time of %1 later than start time "
                    "of %2, therefore backup is not running.")
                    .arg(backupEndTimeStr).arg(backupStartTimeStr));
            return false;
        }
        else if (backupStartTime.secsTo(MythDate::current()) > 600)
        {
            LOG(VB_DATABASE, LOG_ERR,
                QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, but has not ended yet.  "
                    "The backup started %2 seconds ago and should have "
                    "finished by now therefore it appears it is not running")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(MythDate::current())));
            return false;
        }
        else
        {
            // start > end and started less than 10 minutes ago
            LOG(VB_DATABASE, LOG_INFO, QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, and is still running.")
                    .arg(backupStartTimeStr));
            return true;
        }
    }

    // Shouldn't get here
    return false;
}

/** \fn DBUtil::BackupDB(QString&, bool)
 *  \brief Requests a backup of the database.
 *
 *   If the DatabaseBackupScript exists in the ShareDir, it will be executed.
 *   All required database information will be made available as name=value
 *   pairs in a temporary file whose filename will be passed to the backup
 *   script.  The script may parse this file to obtain the required information
 *   to run a backup program, such as mysqldump or mysqlhotcopy.
 *
 *   If the DatabaseBackupScript does not exist, a backup will be performed
 *   using mysqldump directly.  The database password will be passed in a
 *   temporary file so it does not have to be specified on the command line.
 *
 *   Care should be taken in calling this function.  It has the potential to
 *   corrupt in-progress recordings or interfere with playback.
 *
 *   The disableRotation argument should be used only for automatic backups
 *   when users could lose important backup files due to a failure loop--for
 *   example, a DB upgrade failure and a distro start script that keeps
 *   restarting mythbackend even when it exits with an error status.
 *
 *  \param filename        Used to return the name of the resulting backup file
 *  \param disableRotation Disable backup rotation
 *  \return                MythDBBackupStatus indicating the result
 */
MythDBBackupStatus DBUtil::BackupDB(QString &filename, bool disableRotation)
{
    filename = QString();

#ifdef _WIN32
    LOG(VB_GENERAL, LOG_CRIT, "Database backups disabled on Windows.");
    return kDB_Backup_Disabled;
#else

    if (gCoreContext->GetNumSetting("DisableAutomaticBackup", 0))
    {
        LOG(VB_GENERAL, LOG_CRIT, 
            "Database backups disabled.  Skipping backup.");
        return kDB_Backup_Disabled;
    }

    if (IsNewDatabase())
    {
        LOG(VB_GENERAL, LOG_CRIT, "New database detected.  Skipping backup.");
        return kDB_Backup_Empty_DB;
    }

    QString backupScript = GetShareDir() + "mythconverg_backup.pl";
    backupScript = gCoreContext->GetSetting("DatabaseBackupScript",
                                            backupScript);

    if (!QFile::exists(backupScript))
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("Database backup script does "
                                          "not exist: %1").arg(backupScript));
        backupScript = QString::null;
    }

    bool result = false;
    MSqlQuery query(MSqlQuery::InitCon());

    gCoreContext->SaveSettingOnHost(
        "BackupDBLastRunStart",
        MythDate::toString(MythDate::current(), MythDate::kDatabase), NULL);

    if (!backupScript.isEmpty())
    {
        result = DoBackup(backupScript, filename, disableRotation);
        if (!result)
            LOG(VB_GENERAL, LOG_CRIT, "Script-based database backup failed. "
                                      "Retrying with internal backup.");
    }

    if (!result)
        result = DoBackup(filename);

    gCoreContext->SaveSettingOnHost(
        "BackupDBLastRunEnd",
        MythDate::toString(MythDate::current(), MythDate::kDatabase), NULL);

    if (query.isConnected())
    {
        QString dbTag("BackupDB");
        query.prepare("DELETE FROM housekeeping WHERE tag = :TAG ;");
        query.bindValue(":TAG", dbTag);
        if (!query.exec())
            MythDB::DBError("DBUtil::BackupDB", query);

        query.prepare("INSERT INTO housekeeping(tag,lastrun) "
                       "values(:TAG ,now()) ;");
        query.bindValue(":TAG", dbTag);
        if (!query.exec())
            MythDB::DBError("DBUtil::BackupDB", query);
    }

    if (result)
        return kDB_Backup_Completed;

    return kDB_Backup_Failed;
#endif // _WIN32
}

/** \fn DBUtil::CheckTables(const bool repair, const QString options)
 *  \brief Checks database tables
 *
 *   This function will check database tables.
 *
 *  \param repair Repair any tables whose status is not OK
 *  \param options Options to be passed to CHECK TABLE; defaults to QUICK
 *  \return false if any tables have status other than OK; if repair is true,
 *          returns true if those tables were repaired successfully
 *  \sa DBUtil::RepairTables(const QStringList)
 */
bool DBUtil::CheckTables(const bool repair, const QString options)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return false;

    const QStringList all_tables = GetTables(QStringList("MyISAM"));

    if (all_tables.empty())
        return true;

    QString sql = QString("CHECK TABLE %1 %2;").arg(all_tables.join(", "))
                                               .arg(options);

    LOG(VB_GENERAL, LOG_CRIT, "Checking database tables.");
    if (!query.exec(sql))
    {
        MythDB::DBError("DBUtil Checking Tables", query);
        return false;
    }

    QStringList tables = CheckRepairStatus(query);
    bool result = true;
    if (!tables.empty())
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("Found crashed database table(s): %1")
                                      .arg(tables.join(", ")));
        if (repair == true)
            // If RepairTables() repairs the crashed tables, return true
            result = RepairTables(tables);
        else
            result = false;
    }

    return result;
}

/** \fn DBUtil::RepairTables(const QStringList &tables)
 *  \brief Repairs database tables
 *
 *   This function will repair MyISAM database tables.
 *
 *   Care should be taken in calling this function. It should only be called
 *   when no clients are accessing the database, and in the event the MySQL
 *   server crashes, it is critical that a REPAIR TABLE is run on the table
 *   that was being processed at the time of the server crash before any other
 *   operations are performed on that table, or the table may be destroyed. It
 *   is up to the caller of this function to guarantee the safety of performing
 *   database repairs.
 *
 *  \param tables List of tables to repair
 *  \return false if errors were encountered repairing tables
 *  \sa DBUtil::CheckTables(const bool, const QString)
 */
bool DBUtil::RepairTables(const QStringList &tables)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return false;

    QString all_tables = tables.join(", ");
    LOG(VB_GENERAL, LOG_CRIT, QString("Repairing database tables: %1")
                                  .arg(all_tables));

    QString sql = QString("REPAIR TABLE %1;").arg(all_tables);
    if (!query.exec(sql))
    {
        MythDB::DBError("DBUtil Repairing Tables", query);
        return false;
    }

    QStringList bad_tables = CheckRepairStatus(query);
    bool result = true;
    if (!bad_tables.empty())
    {
        LOG(VB_GENERAL, LOG_CRIT, 
            QString("Unable to repair crashed table(s): %1")
                .arg(bad_tables.join(", ")));
        result = false;
    }
    return result;
}

/** \fn DBUtil::CheckRepairStatus(MSqlQuery &query)
 *  \brief Parse the results of a CHECK TABLE or REPAIR TABLE run.
 *
 *   This function reads the records returned by a CHECK TABLE or REPAIR TABLE
 *   run and determines the status of the table(s). The query should have
 *   columns Table, Msg_type, and Msg_text.
 *
 *   The function properly handles multiple records for a single table. If the
 *   last record for a given table shows a status (Msg_type) of OK (Msg_text),
 *   the table is considered OK, even if an error or warning appeared before
 *   (this could be the case, for example, when an empty table is crashed).
 *
 *  \param query An already-executed CHECK TABLE or REPAIR TABLE query whose
 *               results should be parsed.
 *  \return A list of names of not-OK (errored or crashed) tables
 *  \sa DBUtil::CheckTables(const bool, const QString)
 *  \sa DBUtil::RepairTables(const QStringList)
 */
QStringList DBUtil::CheckRepairStatus(MSqlQuery &query)
{
    QStringList tables;
    QSqlRecord record = query.record();
    int table_index = record.indexOf("Table");
    int type_index = record.indexOf("Msg_type");
    int text_index = record.indexOf("Msg_text");
    QString table, type, text, previous_table;
    bool ok = true;
    while (query.next())
    {
        table = query.value(table_index).toString();
        type = query.value(type_index).toString();
        text = query.value(text_index).toString();
        if (table != previous_table)
        {
            if (!ok)
            {
                tables.append(previous_table);
                ok = true;
            }
            previous_table = table;
        }
        // If the final row shows status OK, the table is now good
        if ("status" == type.toLower() && "ok" == text.toLower())
            ok = true;
        else if ("error" == type.toLower() ||
                 ("status" == type.toLower() && "ok" != text.toLower()))
            ok = false;
    }
    // Check the last table in the list
    if (!ok)
        tables.append(table);
    return tables;
}

/** \fn DBUtil::GetTables(const QStringList &engines)
 *  \brief Retrieves a list of tables from the database.
 *
 *  \return QStringList containing table names
 */
QStringList DBUtil::GetTables(const QStringList &engines)
{
    QStringList result;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return result;

    QString sql = "SELECT CONCAT('`', INFORMATION_SCHEMA.TABLES.TABLE_SCHEMA, "
                  "              '`.`', INFORMATION_SCHEMA.TABLES.TABLE_NAME, "
                  "              '`') AS `TABLE_NAME` "
                  "  FROM INFORMATION_SCHEMA.TABLES "
                  " WHERE INFORMATION_SCHEMA.TABLES.TABLE_SCHEMA = DATABASE() "
                  "   AND INFORMATION_SCHEMA.TABLES.TABLE_TYPE = 'BASE TABLE'";
    if (!engines.empty())
        sql.append(QString("   AND INFORMATION_SCHEMA.TABLES.ENGINE IN ('%1')")
                           .arg(engines.join("', '")));
    if (!query.exec(sql))
    {
        MythDB::DBError("DBUtil Finding Tables", query);
        return result;
    }

    while (query.next())
    {
        result.append(query.value(0).toString());
    }

    return result;
}

/** \fn DBUtil::CreateBackupFilename(QString prefix, QString extension)
 *  \brief Creates a filename to use for the filename.
 *
 *   The filename is a concatenation of the given prefix, a hyphen, the current
 *   date/time, and the extension.
 *
 *  \param prefix The prefix (i.e. a database name) which should appear before
 *                the date/time
 *  \param extension The extension to use for the file, including a dot, if
 *                   desired
 *  \return QString name
 */
QString DBUtil::CreateBackupFilename(QString prefix, QString extension)
{
    QString time = MythDate::toString(MythDate::current(), MythDate::kFilename);
    return QString("%1-%2%3").arg(prefix).arg(time).arg(extension);
}

/** \fn DBUtil::GetBackupDirectory(void)
 *  \brief Determines the appropriate path for the database backup.
 *
 *   The function requests the special "DB Backups" storage group.  In the
 *   event the group is not defined, the StorageGroup will fall back to using
 *   the "Default" group.  For users upgrading from version 0.20 or before
 *   (which do not support Storage Groups), the StorageGroup will fall back to
 *   using the old RecordFilePrefix.
 */
QString DBUtil::GetBackupDirectory()
{
    QString directory;
    StorageGroup sgroup("DB Backups", gCoreContext->GetHostName());
    QStringList dirList = sgroup.GetDirList();
    if (dirList.size())
    {
        directory = sgroup.FindNextDirMostFree();

        if (!QDir(directory).exists())
        {
            LOG(VB_FILE, LOG_INFO, "GetBackupDirectory() - ignoring " +
                                   directory + ", using /tmp");
            directory = QString::null;
        }
    }

    if (directory.isNull())
        // Rather than use kDefaultStorageDir, the default for
        // FindNextDirMostFree() when no dirs are defined for the StorageGroup,
        // use /tmp as it's possible that kDefaultStorageDir doesn't exist
        // and (at least on *nix) less possible that /tmp doesn't exist
        directory = "/tmp";

    return directory;
}

/** \brief Creates temporary file containing sensitive DB info.
 *
 *   So we don't have to specify the password on the command line, use
 *   --defaults-extra-file to specify a temporary file with a [client] and
 *  [mysqldump] section that provides the password.  This will fail if the
 *   user's ~/.my.cnf (which is read after the --defaults-extra-file)
 *  specifies a different password that's incorrect for dbUserName
 */
bool DBUtil::CreateTemporaryDBConf(
    const QString &privateinfo, QString &filename)
{
    bool ok = true;
    filename = createTempFile("/tmp/mythtv_db_backup_conf_XXXXXX");
    const QByteArray     tmpfile     = filename.toLocal8Bit();
    const DatabaseParams dbParams    = gCoreContext->GetDatabaseParams();

    FILE *fp = fopen(tmpfile.constData(), "w");
    if (!fp)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Unable to create temporary "
                    "configuration file for creating DB backup: %1")
                .arg(tmpfile.constData()));
        filename = "";
        ok = false;
    }
    else
    {
        if (chmod(tmpfile.constData(), S_IRUSR))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error changing permissions '%1'")
                    .arg(tmpfile.constData()) + ENO);
        }

        QByteArray outarr = privateinfo.toLocal8Bit();
        fprintf(fp, "%s", outarr.constData());

        if (fclose(fp))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error closing '%1'")
                    .arg(tmpfile.constData()) + ENO);
        }
    }

    return ok;
}

/** \fn DBUtil::DoBackup(const QString&, QString&)
 *  \brief Creates a backup of the database by executing the backupScript.
 *
 *   This function executes the specified backup script to create a database
 *   backup.  This is the preferred approach for creating the backup.
 */
bool DBUtil::DoBackup(const QString &backupScript, QString &filename,
                      bool disableRotation)
{
    DatabaseParams dbParams = gCoreContext->GetDatabaseParams();
    QString     dbSchemaVer = gCoreContext->GetSetting("DBSchemaVer");
    QString backupDirectory = GetBackupDirectory();
    QString  backupFilename = CreateBackupFilename(dbParams.dbName + "-" +
                                                   dbSchemaVer, ".sql");
    QString      scriptArgs = gCoreContext->GetSetting("BackupDBScriptArgs");
    QString rotate = "";
    if (disableRotation)
    {
        if (!(scriptArgs.contains("rotate", Qt::CaseInsensitive)))
            rotate = "rotate=-1";
    }


    QString privateinfo =
        QString("DBHostName=%1\nDBPort=%2\n"
                "DBUserName=%3\nDBPassword=%4\n"
                "DBName=%5\nDBSchemaVer=%6\n"
                "DBBackupDirectory=%7\nDBBackupFilename=%8\n%9\n")
        .arg(dbParams.dbHostName).arg(dbParams.dbPort)
        .arg(dbParams.dbUserName).arg(dbParams.dbPassword)
        .arg(dbParams.dbName).arg(dbSchemaVer)
        .arg(backupDirectory).arg(backupFilename).arg(rotate);
    QString tempDatabaseConfFile = QString::null;
    bool hastemp = CreateTemporaryDBConf(privateinfo, tempDatabaseConfFile);
    if (!hastemp)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Attempting backup, anyway.");

    LOG(VB_GENERAL, LOG_ERR, QString("Backing up database with script: '%1'")
            .arg(backupScript));

    QString command = backupScript + " " + scriptArgs + " " +
                      tempDatabaseConfFile;
    uint status = myth_system(command, kMSDontBlockInputDevs|kMSAnonLog);

    if (hastemp)
    {
        QByteArray tmpfile = tempDatabaseConfFile.toLocal8Bit();
        unlink(tmpfile.constData());
    }

    if (status != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error backing up database: %1 (%2)")
                .arg(command).arg(status));
        filename = "__FAILED__";
        return false;
    }

    LOG(VB_GENERAL, LOG_CRIT, "Database Backup complete.");

    QDir dir(backupDirectory, backupFilename + "*");
    uint numfiles = dir.count();
    if (numfiles < 1)
    {
        // If no file begins with the suggested filename, don't show the backup
        // filename in the GUI message -- the script probably used some other
        // filename
        filename = "";
        LOG(VB_FILE, LOG_ERR, LOC +
            QString("No files beginning with the suggested database backup "
                    "filename '%1' were found in '%2'.")
                .arg(backupFilename).arg(backupDirectory));
    }
    else
    {
        filename = dir.path() + "/" + dir[0];;
        if (numfiles > 1)
        {
            LOG(VB_FILE, LOG_ERR, LOC +
                QString("Multiple files beginning with the suggested database "
                        "backup filename '%1' were found in '%2'. "
                        "Assuming the first is the backup.")
                    .arg(backupFilename).arg(backupDirectory));
        }
    }

    if (!filename.isEmpty())
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("Backed up database to file: '%1'")
                .arg(filename));
    }

    return true;
}

/** \fn DBUtil::DoBackup(QString&)
 *  \brief Creates a backup of the database.
 *
 *   This fallback function is used only if the database backup script cannot
 *   be found.
 */
bool DBUtil::DoBackup(QString &filename)
{
    DatabaseParams dbParams = gCoreContext->GetDatabaseParams();
    QString     dbSchemaVer = gCoreContext->GetSetting("DBSchemaVer");
    QString backupDirectory = GetBackupDirectory();

    QString command;
    QString compressCommand("");
    QString extension = ".sql";
    if (QFile::exists("/bin/gzip"))
        compressCommand = "/bin/gzip";
    else if (QFile::exists("/usr/bin/gzip"))
        compressCommand = "/usr/bin/gzip";
    else
        LOG(VB_GENERAL, LOG_CRIT, "Neither /bin/gzip nor /usr/bin/gzip exist. "
                                  "The database backup will be uncompressed.");

    QString backupFilename = CreateBackupFilename(
        dbParams.dbName + "-" + dbSchemaVer, extension);
    QString backupPathname = backupDirectory + "/" + backupFilename;

    QString privateinfo = QString(
        "[client]\npassword=%1\n[mysqldump]\npassword=%2\n")
        .arg(dbParams.dbPassword).arg(dbParams.dbPassword);
    QString tempExtraConfFile = QString::null;
    if (!CreateTemporaryDBConf(privateinfo, tempExtraConfFile))
        return false;

    QString portArg = "";
    if (dbParams.dbPort > 0)
        portArg = QString(" --port='%1'").arg(dbParams.dbPort);
    command = QString("mysqldump --defaults-extra-file='%1' --host='%2'%3"
                      " --user='%4' --add-drop-table --add-locks"
                      " --allow-keywords --complete-insert"
                      " --extended-insert --lock-tables --no-create-db --quick"
                      " '%5' > '%6' 2>/dev/null")
                      .arg(tempExtraConfFile).arg(dbParams.dbHostName)
                      .arg(portArg).arg(dbParams.dbUserName)
                      .arg(dbParams.dbName).arg(backupPathname);

    LOG(VB_FILE, LOG_INFO, QString("Backing up database with command: '%1'")
            .arg(command));
    LOG(VB_GENERAL, LOG_CRIT, QString("Backing up database to file: '%1'")
            .arg(backupPathname));

    uint status = myth_system(command, kMSDontBlockInputDevs|kMSAnonLog);

    QByteArray tmpfile = tempExtraConfFile.toLocal8Bit();
    unlink(tmpfile.constData());

    if (status != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Error backing up database: '%1' (%2)")
                .arg(command).arg(status));
        filename = "__FAILED__";
        return false;
    }

    if (compressCommand != "")
    {
        LOG(VB_GENERAL, LOG_CRIT, "Compressing database backup file.");
        compressCommand += " " + backupPathname;
        status = myth_system(compressCommand, kMSDontBlockInputDevs);

        if (status != GENERIC_EXIT_OK)
        {
            LOG(VB_GENERAL, LOG_CRIT,
                   "Compression failed, backup file will remain uncompressed.");
        }
        else
        {
            backupPathname += ".gz";

            LOG(VB_GENERAL, LOG_CRIT, QString("Database Backup filename: '%1'")
                    .arg(backupPathname));
        }
    }

    LOG(VB_GENERAL, LOG_CRIT, "Database Backup complete.");

    filename = backupPathname;
    return true;
}

/** \fn DBUtil::QueryDBMSVersion(void)
 *  \brief Reads and returns the QString version name from the DBMS or
 *         returns QString::null in the event of an error.
 */
bool DBUtil::QueryDBMSVersion(void)
{
    // Allow users to override the string provided by the database server in
    // case the value was changed to an unrecognizable string by whomever
    // compiled the MySQL server
    QString dbmsVersion = gCoreContext->GetSetting("DBMSVersionOverride");

    if (dbmsVersion.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT VERSION();");
        if (!query.exec() || !query.next())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + 
                "Unable to determine MySQL version.");
            MythDB::DBError("DBUtil Querying DBMS version", query);
            dbmsVersion = QString::null;
        }
        else
            dbmsVersion = query.value(0).toString();
    }
    m_versionString = dbmsVersion;

    return !m_versionString.isEmpty();
}

/** \fn DBUtil::ParseDBMSVersion(void)
 *  \brief Parses m_versionString to find the major, minor, and point version.
 */
bool DBUtil::ParseDBMSVersion()
{
    if (m_versionString.isEmpty())
        if (!QueryDBMSVersion())
            return false;

    bool ok;
    QString section;
    int pos = 0, i = 0;
    int version[3] = {-1, -1, -1};
    QRegExp digits("(\\d+)");

    while ((i < 3) && ((pos = digits.indexIn(m_versionString, pos)) > -1))
    {
        section = digits.cap(1);
        pos += digits.matchedLength();
        version[i] = section.toInt(&ok, 10);
        if (!ok)
            version[i] = -1;
        i++;
    }

    m_versionMajor = version[0];
    m_versionMinor = version[1];
    m_versionPoint = version[2];

    return m_versionMajor > -1;
}

/**
 * Estimate the number of MythTV programs using the database
 */
int DBUtil::CountClients(void)
{
    int count = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
    {
        LOG(VB_GENERAL, LOG_DEBUG, "Not connected to DB");
        return count;
    }

    if (!query.exec("SHOW PROCESSLIST;"))
    {
        MythDB::DBError("DBUtil CountClients", query);
        return count;
    }

    QSqlRecord record = query.record();
    int db_index = record.indexOf("db");
    QString dbName = gCoreContext->GetDatabaseParams().dbName;
    QString inUseDB;

    while (query.next())
    {
        inUseDB = query.value(db_index).toString();
        if (inUseDB == dbName)
            ++count;
    }

    // On average, each myth program has 4 database connections,
    // but we round up just in case a new program is loading:
    count = (count + 3)/4;

    LOG(VB_GENERAL, LOG_DEBUG,
            QString("DBUtil::CountClients() found %1").arg(count));

    return count;
}

/** \brief Try to get a lock on the table schemalock.
 *  Prevents multiple upgrades by different programs of the same schema.
 */
bool DBUtil::TryLockSchema(MSqlQuery &query, uint timeout_secs)
{
    query.prepare("SELECT GET_LOCK('schemaLock', :TIMEOUT)");
    query.bindValue(":TIMEOUT", timeout_secs);
    return query.exec() && query.first() && query.value(0).toBool();
}

void DBUtil::UnlockSchema(MSqlQuery &query)
{
    query.prepare("SELECT RELEASE_LOCK('schemaLock')");
    if (!query.exec())
    {
        MythDB::DBError("DBUtil UnlockSchema", query);
    }
}

/** \fn CheckTimeZoneSupport(void)
 *  \brief Check if MySQL has working timz zone support.
 */
bool DBUtil::CheckTimeZoneSupport(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT CONVERT_TZ(NOW(), 'SYSTEM', 'Etc/UTC')");
    if (!query.exec() || !query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, "MySQL time zone support check failed");
        return false;
    }

    return !query.value(0).isNull();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
