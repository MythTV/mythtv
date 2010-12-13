#include <unistd.h>      // for isatty() on Windows

#include "dialogbox.h"
#include "mythcorecontext.h"
#include "schemawizard.h"
#include "util.h"

#include "mythtimer.h"
#include "mythverbose.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythdb.h"


static SchemaUpgradeWizard * c_wizard = 0;


SchemaUpgradeWizard::SchemaUpgradeWizard(const QString &DBSchemaSetting,
                                         const QString &appName,
                                         const QString &upgradeSchemaVal)
    : DBver(), emptyDB(false), versionsBehind(-1),
      backupStatus(kDB_Backup_Unknown),
      m_autoUpgrade(false),
      m_backupResult(),
      m_busyPopup(NULL),
      m_expertMode(false),
      m_schemaSetting(DBSchemaSetting),
      m_schemaName(appName),
      m_newSchemaVer(upgradeSchemaVal)
{
    c_wizard = this;

    // Users and developers can choose to live dangerously,
    // either to silently and automatically upgrade,
    // or an expert option to allow use of existing:
    switch (gCoreContext->GetNumSetting("DBSchemaAutoUpgrade"))
    {
        case  1: m_autoUpgrade = true; break;
        case -1: m_expertMode  = true; break;
        default: break;
    }
}

SchemaUpgradeWizard::~SchemaUpgradeWizard()
{
    c_wizard = 0;
}

SchemaUpgradeWizard *
SchemaUpgradeWizard::Get(const QString &DBSchemaSetting,
                         const QString &appName,
                         const QString &upgradeSchemaVal)
{
    if (c_wizard == 0)
        c_wizard = new SchemaUpgradeWizard(DBSchemaSetting, appName,
                                           upgradeSchemaVal);
    else
    {
        c_wizard->DBver            = QString();
        c_wizard->versionsBehind   = -1;
        c_wizard->m_schemaSetting  = DBSchemaSetting;
        c_wizard->m_schemaName     = appName;
        c_wizard->m_newSchemaVer   = upgradeSchemaVal;
    }

    return c_wizard;
}

/**
 * Delete any current "busy" popup, create new one.
 *
 * Wish there was a way to change the message on existing popup.
 */
void SchemaUpgradeWizard::BusyPopup(const QString &message)
{
    if (m_busyPopup)
        m_busyPopup->Close();

    m_busyPopup = ShowBusyPopup(message);
}

MythDBBackupStatus SchemaUpgradeWizard::BackupDB(void)
{
    if (emptyDB)
    {
        VERBOSE(VB_GENERAL,
                "The database seems to be empty - not attempting a backup");
        return kDB_Backup_Empty_DB;
    }

    backupStatus = DBUtil::BackupDB(m_backupResult);

    return backupStatus;
}

int SchemaUpgradeWizard::Compare(void)
{
    DBver = gCoreContext->GetSetting(m_schemaSetting);

    // No current schema? Investigate further:
    if (DBver.isEmpty() || DBver == "0")
    {
        VERBOSE(VB_GENERAL, "No current database version?");

        if (DBUtil::IsNewDatabase())
        {
            VERBOSE(VB_GENERAL, "Database appears to be empty/new!");
            emptyDB = true;
        }
    }
    else
        VERBOSE(VB_GENERAL, QString("Current %1 Schema Version (%2): %3")
                                    .arg(m_schemaName).arg(m_schemaSetting)
                                    .arg(DBver));

#if TESTING
    //DBver = "9" + DBver + "-testing";
    DBver += "-testing";
return 0;
#endif

    return versionsBehind = m_newSchemaVer.toInt() - DBver.toUInt();
}

int SchemaUpgradeWizard::CompareAndWait(const int seconds)
{
    if (Compare() > 0)  // i.e. if DB is older than expected
    {
        QString message = tr("%1 database schema is old. Waiting to see if DB "
                             "is being upgraded.").arg(m_schemaName);

        VERBOSE(VB_IMPORTANT, message);

        MSqlQuery query(MSqlQuery::InitCon());
        bool      backupRunning  = false;
        bool      upgradeRunning = false;

        MythTimer elapsedTimer;
        elapsedTimer.start();
        while (versionsBehind && (elapsedTimer.elapsed() < seconds * 1000))
        {
            sleep(1);

            if (IsBackupInProgress())
            {
                VERBOSE(VB_IMPORTANT,
                        "Waiting for Database Backup to complete.");
                if (!backupRunning)
                {
                    elapsedTimer.restart();
                    backupRunning = true;
                }
                continue;
            }

            if (!lockSchema(query))
            {
                VERBOSE(VB_IMPORTANT,
                        "Waiting for Database Upgrade to complete.");
                if (!upgradeRunning)
                {
                    elapsedTimer.restart();
                    upgradeRunning = true;
                }
                continue;
            }

            Compare();
            unlockSchema(query);

            if (m_expertMode)
                break;
        }

        if (versionsBehind)
            VERBOSE(VB_IMPORTANT, "Timed out waiting.");
        else
            VERBOSE(VB_IMPORTANT,
                    "Schema version was upgraded while we were waiting.");
    }
    // else DB is same version, or newer. Either way, we won't upgrade it

    return versionsBehind;
}

MythSchemaUpgrade SchemaUpgradeWizard::GuiPrompt(const QString &message,
                                                 bool upgradable, bool expert)
{
    DialogBox       * dlg;
    MythMainWindow  * win = GetMythMainWindow();

    if (!win)
        return MYTH_SCHEMA_ERROR;

    dlg = new DialogBox(win, message);
    dlg->AddButton(tr("Exit"));
    if (upgradable)
        dlg->AddButton(tr("Upgrade"));
    if (expert)
        dlg->AddButton(tr("Use current schema"));

    DialogCode selected = dlg->exec();
    dlg->deleteLater();

    switch (selected)
    {
        case kDialogCodeRejected:
        case kDialogCodeButton0:
            return MYTH_SCHEMA_EXIT;
        case kDialogCodeButton1:
            return upgradable ? MYTH_SCHEMA_UPGRADE: MYTH_SCHEMA_USE_EXISTING;
        case kDialogCodeButton2:
            return MYTH_SCHEMA_USE_EXISTING;
        default:
            return MYTH_SCHEMA_ERROR;
    }
}

/**
 * Tell the user that a schema needs to be upgraded, ask if that's OK,
 * remind them about backups, et c.  The GUI buttons default to Exit.
 * The shell command prompting requires an explicit "yes".
 *
 * \param name            What schema are we planning to upgrade? (TV? Music?)
 * \param upgradeAllowed  In not true, and DBSchemaAutoUpgrade isn't set for
 *                        expert mode, this is just a few information messages
 * \param upgradeIfNoUI   Default for non-interactive shells
 *
 * \todo Clarify whether the minDBMS stuff is just for upgrading,
 *       or if it is a runtime requirement too. If the latter,
 *       then this possibly should be called even if the schema match,
 *       to ensure the user is informed of the MySQL upgrade requirement.
 *
 * \todo This uses GetMythUI()->IsScreenSetup() to work out if this program's
 *       context is a GUI, but GetMythUI() might create a MythUIHelper.
 *       Having a static bool MythUIHelper::ValidMythUI() would be much tidier?
 */
enum MythSchemaUpgrade
SchemaUpgradeWizard::PromptForUpgrade(const char *name,
                                      const bool upgradeAllowed,
                                      const bool upgradeIfNoUI,
                                      const int  minDBMSmajor,
                                      const int  minDBMSminor,
                                      const int  minDBMSpoint)
{
    bool     connections;   // Are (other) FE/BEs connected?
    bool     gui;           // Was gContext Init'ed gui=true?
    bool     upgradable;    // Can/should we upgrade?
    bool     validDBMS;     // Do we measure up to minDBMS* ?
    QString  warnOldDBMS;
    QString  warnOtherCl;



    if (versionsBehind == -1)
        Compare();

#if minDBMS_is_only_for_schema_upgrades
    if (versionsBehind == 0)              // Why was this method even called?
        return MYTH_SCHEMA_USE_EXISTING;
#endif

    // Only back up the database if we haven't already successfully made a
    // backup and the database is old/about to be upgraded (not if it's too
    // new) or if a user is doing something they probably shouldn't ("expert
    // mode")
    if (((backupStatus == kDB_Backup_Unknown) ||
         (backupStatus == kDB_Backup_Failed)) &&
        ((upgradeAllowed && (versionsBehind > 0)) ||
         m_expertMode))
        BackupDB();

    connections = CountClients() > 1;
    gui         = GetMythUI()->IsScreenSetup() && GetMythMainWindow();
    validDBMS   = (minDBMSmajor == 0)   // If the caller provided no version,
                  ? true                // the upgrade code can't be fussy!
                  : CompareDBMSVersion(minDBMSmajor,
                                       minDBMSminor, minDBMSpoint) >= 0;
    upgradable  = validDBMS && (versionsBehind > 0)
                            && (upgradeAllowed || m_expertMode);


    // Build up strings used both in GUI and command shell contexts:
    if (connections)
        warnOtherCl = tr("There are also other clients using this"
                         " database. They should be shut down first.");
    if (!validDBMS)
        warnOldDBMS = tr("Error: This version of Myth%1"
                         " requires MySQL %2.%3.%4 or later."
                         "  You seem to be running MySQL version %5.")
                      .arg(name).arg(minDBMSmajor).arg(minDBMSminor)
                      .arg(minDBMSpoint).arg(GetDBMSVersion());



    //
    // 1. Deal with the trivial cases (No user prompting required)
    //
    if (validDBMS)
    {
        // Empty database? Always upgrade, to create tables
        if (emptyDB)
            return MYTH_SCHEMA_UPGRADE;

        if (m_autoUpgrade && !connections && upgradable)
            return MYTH_SCHEMA_UPGRADE;
    }

    if (!gui && (!isatty(fileno(stdin)) || !isatty(fileno(stdout))))
    {
        VERBOSE(VB_GENERAL, "Console is non-interactive, can't prompt user...");

        if (m_expertMode)
        {
            VERBOSE(VB_IMPORTANT, "Using existing schema.");
            return MYTH_SCHEMA_USE_EXISTING;
        }

        if (!validDBMS)
        {
            VERBOSE(VB_IMPORTANT, warnOldDBMS);
            return MYTH_SCHEMA_EXIT;
        }

        if (versionsBehind < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("Error: MythTV database has newer "
                    "%1 schema (%2) than expected (%3).")
                    .arg(name).arg(DBver).arg(m_newSchemaVer));
            return MYTH_SCHEMA_ERROR;
        }

        if (upgradeIfNoUI && validDBMS)
        {
            VERBOSE(VB_IMPORTANT, "Upgrading.");
            return MYTH_SCHEMA_UPGRADE;
        }

        return MYTH_SCHEMA_EXIT;
    }



    //
    // 2. Build up a compound message to show the user, wait for a response
    //
    enum MythSchemaUpgrade  returnValue = MYTH_SCHEMA_UPGRADE;
    QString                 message;

    if (upgradable)
    {
        if (m_autoUpgrade && connections)
        {
            message = tr("Error: MythTV cannot upgrade the schema of this"
                         " datatase because other clients are using it.\n\n"
                         "Please shut them down before upgrading.");
            returnValue = MYTH_SCHEMA_ERROR;
        }
        else
        {
            message = tr("Warning: MythTV wants to upgrade your database,")
                      + "\n" + tr("for the %1 schema, from %2 to %3.");
            if (m_expertMode)
                message += "\n\n" +
                           tr("You can try using the old schema,"
                              " but that may cause problems.");
        }
    }
    else if (!validDBMS)
    {
        message = warnOldDBMS;
        returnValue = MYTH_SCHEMA_ERROR;
    }
    else if (versionsBehind > 0)
    {
        message = tr("This version of MythTV requires an updated database. ")
                  + tr("(schema is %1 versions behind)").arg(versionsBehind)
                  + "\n\n" + tr("Please run mythtv-setup or mythbackend "
                                "to update your database.");
        returnValue = MYTH_SCHEMA_ERROR;
    }
    else   // This client is too old
    {
        if (m_expertMode)
            message = tr("Warning: MythTV database has newer"
                         " %1 schema (%2) than expected (%3).");
        else
        {
            message = tr("Error: MythTV database has newer"
                         " %1 schema (%2) than expected (%3).");
            returnValue = MYTH_SCHEMA_ERROR;
        }
    }

    if (backupStatus == kDB_Backup_Failed)
        message += "\n" + tr("MythTV was unable to backup your database.");

    if (message.contains("%1"))
        message = message.arg(name).arg(DBver).arg(m_newSchemaVer);


    DatabaseParams dbParam = MythDB::getMythDB()->GetDatabaseParams();
    message += "\n\n" + tr("Database Host: %1\nDatabase Name: %2")
                        .arg(dbParam.dbHostName).arg(dbParam.dbName);

    if (gui)
    {
        if (returnValue == MYTH_SCHEMA_ERROR)
        {
            // Display error, return warning to caller
            MythPopupBox::showOkPopup(GetMythMainWindow(), "", message);
            return MYTH_SCHEMA_ERROR;
        }

        returnValue = GuiPrompt(message, upgradable, m_expertMode);
        if (returnValue == MYTH_SCHEMA_EXIT)
            return MYTH_SCHEMA_EXIT;

        if (m_expertMode)
            return returnValue;

        // The annoying extra confirmation:
        if (backupStatus == kDB_Backup_Completed)
        {
            int dirPos = m_backupResult.lastIndexOf('/');
            QString dirName;
            QString fileName;
            if (dirPos > 0)
            {
                fileName = m_backupResult.mid(dirPos + 1);
                dirName  = m_backupResult.left(dirPos);
            }
            message = tr("If your system becomes unstable, a database"
                         " backup file called\n%1\nis located in %2")
                      .arg(fileName).arg(dirName);
        }
        else
            message = tr("This cannot be un-done, so having a"
                         " database backup would be a good idea.");
        if (connections)
            message += "\n\n" + warnOtherCl;

        return GuiPrompt(message, upgradable, m_expertMode);
    }

    // We are not in a GUI environment, so try to prompt the user in the shell
    QString resp;

    cout << endl << message.toLocal8Bit().constData() << endl << endl;

    if (returnValue == MYTH_SCHEMA_ERROR)
        return MYTH_SCHEMA_ERROR;

    if (backupStatus == kDB_Backup_Failed)
        cout << "WARNING: MythTV was unable to backup your database."
             << endl << endl;
    else if ((backupStatus == kDB_Backup_Completed) &&
             (m_backupResult != ""))
        cout << "If your system becomes unstable, "
                "a database backup is located in "
             << m_backupResult.toLocal8Bit().constData() << endl << endl;

    if (m_expertMode)
    {
        resp = getResponse("Would you like to use the existing schema?", "yes");
        if (resp.isEmpty() || resp.left(1).toLower() == "y")
            return MYTH_SCHEMA_USE_EXISTING;
    }

    resp = getResponse("\nShall I upgrade this database?", "yes");
    if (!resp.isEmpty() && resp.left(1).toLower() != "y")
        return MYTH_SCHEMA_EXIT;

    if (connections)
        cout << endl << warnOtherCl.toLocal8Bit().constData() << endl;

    if ((backupStatus != kDB_Backup_Completed) &&
        (backupStatus != kDB_Backup_Empty_DB))
    {
        resp = getResponse("\nA database backup might be a good idea"
                           "\nAre you sure you want to upgrade?", "no");
        if (resp.isEmpty() || resp.left(1).toLower() == "n")
            return MYTH_SCHEMA_EXIT;
    }

    return MYTH_SCHEMA_UPGRADE;
}
