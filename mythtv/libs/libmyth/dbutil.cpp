#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <qdir.h>
#include <qfile.h>
#include <qregexp.h>
#include <qdatetime.h>

#include "dbutil.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "storagegroup.h"
#include "util.h"

#define LOC QString("DBUtil: ")
#define LOC_ERR QString("DBUtil Error: ")

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

/** \fn DBUtil::BackupInProgress(void)
 *  \brief Test to see if a DB backup is in progress
 *
 */
bool DBUtil::IsBackupInProgress(void)
{
    QString backupStartTimeStr = gContext->GetSetting("BackupDBLastRunStart");
    QString backupEndTimeStr = gContext->GetSetting("BackupDBLastRunEnd");

    if (backupStartTimeStr.isEmpty())
    {
        VERBOSE(VB_DATABASE, "DBUtil::BackupInProgress(): No start time found, "
                "database backup is not in progress.");
        return false;
    }

    backupStartTimeStr.replace(" ", "T");

    QDateTime backupStartTime =
        QDateTime::fromString(backupStartTimeStr, Qt::ISODate);

    // No end time set
    if (backupEndTimeStr.isEmpty())
    {
        // If DB Backup started less then 10 minutes ago, assume still running
        if (backupStartTime.secsTo(QDateTime::currentDateTime()) < 600)
        {
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): Found "
                    "database backup start time of %1 which was %2 seconds "
                    "ago, therefore it appears the backup is still running.")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(QDateTime::currentDateTime())));
            return true;
        }
        else
        {
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, but no end time was found. "
                    "The backup started %2 seconds ago and should have "
                    "finished by now therefore it appears it is not running .")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(QDateTime::currentDateTime())));
            return false;
        }
    }
    else
    {
        backupEndTimeStr.replace(" ", "T");

        QDateTime backupEndTime =
            QDateTime::fromString(backupEndTimeStr, Qt::ISODate);

        if (backupEndTime >= backupStartTime)
        {
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): Found "
                    "database backup end time of %1 later than start time "
                    "of %2, therefore backup is not running.")
                    .arg(backupEndTimeStr).arg(backupStartTimeStr));
            return false;
        }
        else if (backupStartTime.secsTo(QDateTime::currentDateTime()) > 600)
        {
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, but has not ended yet.  "
                    "The backup started %2 seconds ago and should have "
                    "finished by now therefore it appears it is not running")
                    .arg(backupStartTimeStr)
                    .arg(backupStartTime.secsTo(QDateTime::currentDateTime())));
            return false;
        }
        else
        {
            // start > end and started less than 10 minutes ago
            VERBOSE(VB_DATABASE, QString("DBUtil::BackupInProgress(): "
                    "Database backup started at %1, and is still running.")
                    .arg(backupStartTimeStr));
            return true;
        }
    }

    // Shouldn't get here
    return false;
}

/** \fn DBUtil::BackupDB(QString)
 *  \brief Requests a backup of the database.
 *
 *   Care should be taken in calling this function.  It has the potential to
 *   corrupt in-progress recordings or interfere with playback.
 */
bool DBUtil::BackupDB(QString &filename)
{
    bool result = false;
    MSqlQuery query(MSqlQuery::InitCon());

    gContext->SaveSettingOnHost("BackupDBLastRunStart",
                                QDateTime::currentDateTime()
                                .toString("yyyy-MM-dd hh:mm:ss"), NULL);

    result = DoBackup(filename);

    gContext->SaveSettingOnHost("BackupDBLastRunEnd",
                                QDateTime::currentDateTime()
                                .toString("yyyy-MM-dd hh:mm:ss"), NULL);

    if (query.isConnected())
    {
        QString dbTag("BackupDB");
        query.prepare("DELETE FROM housekeeping WHERE tag = :TAG ;");
        query.bindValue(":TAG", dbTag);
        query.exec();

        query.prepare("INSERT INTO housekeeping(tag,lastrun) "
                       "values(:TAG ,now()) ;");
        query.bindValue(":TAG", dbTag);
        query.exec();
    }

    return result;
}

/** \fn DBUtil::GetTables(void)
 *  \brief Retrieves a list of tables from the database.
 *
 *   The function tries to ensure that the list contains only tables and no
 *   views.  However, for MySQL 5.0.1, the list will also contain any views
 *   defined in the database.
 *
 *  \return QStringList containing table names
 */
QStringList DBUtil::GetTables(void)
{
    QStringList result;
    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        QString sql;
        // MySQL 5.0.2+ support "SHOW FULL TABLES;" to get the Table_type
        // column to distinguish between BASE TABLEs and VIEWs
        bool supportsTableType = (CompareDBMSVersion(5, 0, 2) >= 0);
        if (supportsTableType)
            sql = "SHOW FULL TABLES;";
        else
            sql = "SHOW TABLES;";

        query.prepare(sql);

        if (query.exec() && query.size() > 0)
        {
            while(query.next())
            {
                if (supportsTableType)
                    if (query.value(1).toString() == "VIEW")
                        continue;
                result.append(query.value(0).toString());
            }
        }
        else
            MythContext::DBError("DBUtil Finding Tables", query);
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
    QDateTime now = QDateTime::currentDateTime();
    QString time = now.toString("yyyyMMddhhmmss");
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
    StorageGroup sgroup("DB Backups", gContext->GetHostName());
    QStringList dirList = sgroup.GetDirList();
    if (dirList.size())
    {
        directory = sgroup.FindNextDirMostFree();

        if (!QDir(directory).exists())
        {
            VERBOSE(VB_FILE, "GetBackupDirectory() - ignoring "
                             + directory + ", using /tmp");
            directory = QString::null;
        }
    }

    if (!directory)
        // Rather than use kDefaultStorageDir, the default for
        // FindNextDirMostFree() when no dirs are defined for the StorageGroup,
        // use /tmp as it's possible that kDefaultStorageDir doesn't exist
        // and (at least on *nix) less possible that /tmp doesn't exist
        directory = "/tmp";

    return directory;
}

/** \fn DBUtil::DoBackup(QString)
 *  \brief Creates a backup of the database.
 */
bool DBUtil::DoBackup(QString &filename)
{
    DatabaseParams dbParams = gContext->GetDatabaseParams();
    QString     dbSchemaVer = gContext->GetSetting("DBSchemaVer");
    QString backupDirectory = GetBackupDirectory();

    /* So we don't have to specify the password on the command line, use
       --defaults-extra-file to specify a temporary file with a [client] and
       [mysqldump] section that provides the password.  This will fail if the
       user's ~/.my.cnf (which is read after the --defaults-extra-file)
       specifies a different password that's incorrect for dbUserName */
    QString tempExtraConfFile = QDeepCopy<QString>(
                    createTempFile("/tmp/mythtv_db_backup_conf_XXXXXX"));
    FILE *fp;
    if (!(fp = fopen(tempExtraConfFile.ascii(), "w")))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Unable to create temporary "
                "configuration file for creating DB backup: %1")
                .arg(tempExtraConfFile.ascii()));
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Attempting backup, anyway. "
                "If the backup fails, please add the %1 user's database "
                "password to your MySQL option file.")
                .arg(dbParams.dbUserName));
    }
    else
    {
        chmod(tempExtraConfFile.ascii(), S_IRUSR);
        fprintf(fp, QString("[client]\npassword=%1\n"
                            "[mysqldump]\npassword=%2\n")
                            .arg(dbParams.dbPassword).arg(dbParams.dbPassword));
        if (fclose(fp))
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Error closing %1: %2")
                    .arg(tempExtraConfFile.ascii()).arg(strerror(errno)));
    }

    QString command;
    QString compressCommand("");
    QString extension = ".sql";
    if (QFile::exists("/bin/gzip"))
        compressCommand = "/bin/gzip";
    else if (QFile::exists("/usr/bin/gzip"))
        compressCommand = "/usr/bin/gzip";
    else
        VERBOSE(VB_IMPORTANT, "Neither /bin/gzip nor /usr/bin/gzip exist. "
                              "The database backup will be uncompressed.");

    QString backupPathname = backupDirectory + "/" +
                             CreateBackupFilename(dbParams.dbName + "-" +
                                                  dbSchemaVer, extension);
    command = QString("mysqldump --defaults-extra-file='%1' --host='%2'"
                      " --user='%3' --add-drop-table --add-locks"
                      " --allow-keywords --complete-insert"
                      " --extended-insert --lock-tables --no-create-db --quick"
                      " '%4' > '%5' 2>/dev/null")
                      .arg(tempExtraConfFile).arg(dbParams.dbHostName)
                      .arg(dbParams.dbUserName).arg(dbParams.dbName)
                      .arg(backupPathname);
    VERBOSE(VB_FILE, QString("Backing up database with command: %1")
                             .arg(command.ascii()));
    VERBOSE(VB_IMPORTANT, QString("Backing up database to file: %1")
            .arg(backupPathname.ascii()));
    uint status = system(command.ascii());

    unlink(tempExtraConfFile.ascii());

    if (status)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Error backing up database: %1 (%2)")
                .arg(command.ascii()).arg(status));
        filename = "__FAILED__";
        return false;
    }

    if (compressCommand != "")
    {
        VERBOSE(VB_IMPORTANT, "Compressing database backup file.");
        compressCommand += " " + backupPathname;
        status = system(compressCommand.ascii());

        if (status)
        {
            VERBOSE(VB_IMPORTANT,
                   "Compression failed, backup file will remain uncompressed.");
        }
        else
        {
            backupPathname += ".gz";

            VERBOSE(VB_IMPORTANT, QString("Database Backup filename: %1")
                    .arg(backupPathname.ascii()));
        }
    }

    VERBOSE(VB_IMPORTANT, "Database Backup complete.");

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
    QString dbmsVersion = gContext->GetSetting("DBMSVersionOverride");

    if (dbmsVersion.isEmpty())
    {
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT VERSION();");
        if (!query.exec() || !query.next())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to determine MySQL "
                    "version.");
            MythContext::DBError("DBUtil Querying DBMS version", query);
            dbmsVersion = QString::null;
        }
        else
            dbmsVersion = QString::fromUtf8(query.value(0).toString());
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

    while ((i < 3) && ((pos = digits.search(m_versionString, pos)) > -1))
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

/* vim: set expandtab tabstop=4 shiftwidth=4: */
