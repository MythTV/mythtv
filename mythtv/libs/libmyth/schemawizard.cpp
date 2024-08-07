#include <iostream> // for cout
using std::cout;
using std::endl;

#include <unistd.h>      // for isatty() on Windows

#include <QEventLoop>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythtimer.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythprogressdialog.h"
#include "libmythui/mythuihelper.h"

#include "schemawizard.h"


static SchemaUpgradeWizard * c_wizard = nullptr;


SchemaUpgradeWizard::SchemaUpgradeWizard(QString DBSchemaSetting,
                                         QString appName,
                                         QString upgradeSchemaVal)
    : m_schemaSetting(std::move(DBSchemaSetting)),
      m_schemaName(std::move(appName)),
      m_newSchemaVer(std::move(upgradeSchemaVal))
{
    c_wizard = this;

    // Users and developers can choose to live dangerously,
    // either to silently and automatically upgrade,
    // or an expert option to allow use of existing:
    switch (gCoreContext->GetNumSetting("DBSchemaAutoUpgrade"))
    {
        case  1: m_autoUpgrade = true; break;
#if defined(ENABLE_SCHEMA_DEVELOPER_MODE) && ENABLE_SCHEMA_DEVELOPER_MODE
        case -1: m_expertMode  = true; break;
#endif
        default: break;
    }
}

SchemaUpgradeWizard::~SchemaUpgradeWizard()
{
    c_wizard = nullptr;
}

SchemaUpgradeWizard *
SchemaUpgradeWizard::Get(const QString &DBSchemaSetting,
                         const QString &appName,
                         const QString &upgradeSchemaVal)
{
    if (c_wizard == nullptr)
    {
        c_wizard = new SchemaUpgradeWizard(DBSchemaSetting, appName,
                                           upgradeSchemaVal);
    }
    else
    {
        c_wizard->m_DBver          = QString();
        c_wizard->m_versionsBehind = -1;
        c_wizard->m_schemaSetting  = DBSchemaSetting;
        c_wizard->m_schemaName     = appName;
        c_wizard->m_newSchemaVer   = upgradeSchemaVal;
    }

    return c_wizard;
}

/**
 * Delete any current "busy" popup, create new one.
 */
void SchemaUpgradeWizard::BusyPopup(const QString &message)
{
    if (m_busyPopup)
        m_busyPopup->SetMessage(message);

    m_busyPopup = ShowBusyPopup(message);
}

MythDBBackupStatus SchemaUpgradeWizard::BackupDB(void)
{
    if (m_emptyDB)
    {
        LOG(VB_GENERAL, LOG_INFO,
                 "The database seems to be empty - not attempting a backup");
        return kDB_Backup_Empty_DB;
    }

    m_backupStatus = DBUtil::BackupDB(m_backupResult, true);

    return m_backupStatus;
}

int SchemaUpgradeWizard::Compare(void)
{
    m_DBver = gCoreContext->GetSetting(m_schemaSetting);

    // No current schema? Investigate further:
    if (m_DBver.isEmpty() || m_DBver == "0")
    {
        LOG(VB_GENERAL, LOG_INFO, "No current database version?");

        if (DBUtil::IsNewDatabase())
        {
            LOG(VB_GENERAL, LOG_INFO, "Database appears to be empty/new!");
            m_emptyDB = true;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO,
                 QString("Current %1 Schema Version (%2): %3")
                     .arg(m_schemaName, m_schemaSetting, m_DBver));
    }

#if defined(TESTING) && TESTING
    //m_DBver = "9" + m_DBver + "-testing";
    m_DBver += "-testing";
    return 0;
#endif

    if (m_newSchemaVer == m_DBver)
    {
        m_versionsBehind = 0;
    }
    else
    {
        // Branch DB versions may not be integer version numbers.
        bool new_ok = false;
        bool old_ok = false;
        int new_version = m_newSchemaVer.toInt(&new_ok);
        int old_version = m_DBver.toInt(&old_ok);
        if (new_ok && old_ok)
            m_versionsBehind = new_version - old_version;
        else
            m_versionsBehind = 5000;
    }
    return m_versionsBehind;
}

MythSchemaUpgrade SchemaUpgradeWizard::GuiPrompt(const QString &message,
                                                 bool upgradable, bool expert)
{
    MythMainWindow *win = GetMythMainWindow();
    if (!win)
        return MYTH_SCHEMA_ERROR;

    MythScreenStack *stack = win->GetMainStack();
    if (!stack)
        return MYTH_SCHEMA_ERROR;

    // Ignore MENU & ESCAPE dialog actions
    int btnIndex = -1;
    while (btnIndex < 0)
    {
        auto *dlg = new MythDialogBox(message, stack, "upgrade");
        if (!dlg->Create())
        {
            delete dlg;
            return MYTH_SCHEMA_ERROR;
        }

        dlg->AddButton(tr("Exit"));

        if (upgradable)
            dlg->AddButton(tr("Upgrade"));

        if (expert)
            // Not translated. This string can't appear in released builds.
            dlg->AddButton("Use current schema");

        stack->AddScreen(dlg);

        // Wait in local event loop so events are processed
        QEventLoop block;
        connect(dlg,    &MythDialogBox::Closed,
                &block, [&](const QString& /*resultId*/, int result) { block.exit(result); });

        // Block until dialog closes
        btnIndex = block.exec();
    }

    switch (btnIndex)
    {
    case 0  : return MYTH_SCHEMA_EXIT;
    case 1  : return upgradable ? MYTH_SCHEMA_UPGRADE
                                : MYTH_SCHEMA_USE_EXISTING;
    case 2  : return MYTH_SCHEMA_USE_EXISTING;
    default : break;
    }

    return MYTH_SCHEMA_ERROR;
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
 * \param minDBMSmajor    Required minimum major version of mysql/mariadb.
 * \param minDBMSminor    Required minimum minor version of mysql/mariadb.
 * \param minDBMSpoint    Required minimum point version of mysql/mariadb.
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
    bool     connections = false;   // Are (other) FE/BEs connected?
    bool     gui = false;           // Was gContext Init'ed gui=true?
    bool     upgradable = false;    // Can/should we upgrade?
    bool     validDBMS = false;     // Do we measure up to minDBMS* ?
    QString  warnOldDBMS;
    QString  warnOtherCl;



    if (m_versionsBehind == -1)
        Compare();

#if defined(minDBMS_is_only_for_schema_upgrades) && minDBMS_is_only_for_schema_upgrades
    if (m_versionsBehind == 0)              // Why was this method even called?
        return MYTH_SCHEMA_USE_EXISTING;
#endif

    // Only back up the database if we haven't already successfully made a
    // backup and the database is old/about to be upgraded (not if it's too
    // new) or if a user is doing something they probably shouldn't ("expert
    // mode")
    if (((m_backupStatus == kDB_Backup_Unknown) ||
         (m_backupStatus == kDB_Backup_Failed)) &&
        ((upgradeAllowed && (m_versionsBehind > 0)) ||
         m_expertMode))
        BackupDB();

    connections = CountClients() > 1;
    gui         = GetMythUI()->IsScreenSetup() && GetMythMainWindow();
    validDBMS   = (minDBMSmajor == 0)   // If the caller provided no version,
                  ? true                // the upgrade code can't be fussy!
                  : CompareDBMSVersion(minDBMSmajor,
                                       minDBMSminor, minDBMSpoint) >= 0;
    upgradable  = validDBMS && (m_versionsBehind > 0)
                            && (upgradeAllowed || m_expertMode);


    // Build up strings used both in GUI and command shell contexts:
    if (connections)
        warnOtherCl = tr("There are also other clients using this"
                         " database. They should be shut down first.");
    if (!validDBMS)
    {
        warnOldDBMS = tr("Error: This version of Myth%1"
                         " requires MySQL %2.%3.%4 or later."
                         "  You seem to be running MySQL version %5.")
                      .arg(name).arg(minDBMSmajor).arg(minDBMSminor)
                      .arg(minDBMSpoint).arg(GetDBMSVersion());
    }

    //
    // 1. Deal with the trivial cases (No user prompting required)
    //
    if (validDBMS)
    {
        // Empty database? Always upgrade, to create tables
        if (m_emptyDB)
            return MYTH_SCHEMA_UPGRADE;

        if (m_autoUpgrade && !connections && upgradable)
            return MYTH_SCHEMA_UPGRADE;
    }

    if (!gui && (!isatty(fileno(stdin)) || !isatty(fileno(stdout))))
    {
        LOG(VB_GENERAL, LOG_INFO,
                 "Console is non-interactive, can't prompt user...");

        if (m_expertMode)
        {
            LOG(VB_GENERAL, LOG_CRIT, "Using existing schema.");
            return MYTH_SCHEMA_USE_EXISTING;
        }

        if (!validDBMS)
        {
            LOG(VB_GENERAL, LOG_CRIT, warnOldDBMS);
            return MYTH_SCHEMA_EXIT;
        }

        if (m_versionsBehind < 0)
        {
            LOG(VB_GENERAL, LOG_CRIT,
                     QString("Error: MythTV database has newer %1 schema (%2) "
                             "than expected (%3).")
                         .arg(name, m_DBver, m_newSchemaVer));
            return MYTH_SCHEMA_ERROR;
        }

        if (upgradeIfNoUI && validDBMS)
        {
            LOG(VB_GENERAL, LOG_CRIT, "Upgrading.");
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
            {
                // Not translated. This string can't appear in released builds.
                message += "\n\nYou can try using the old schema,"
                           " but that may cause problems.";
            }
        }
    }
    else if (!validDBMS)
    {
        message = warnOldDBMS;
        returnValue = MYTH_SCHEMA_ERROR;
    }
    else if (m_versionsBehind > 0)
    {
        message = tr("This version of MythTV requires an updated database. ")
                  + tr("(schema is %1 versions behind)").arg(m_versionsBehind)
                  + "\n\n" + tr("Please run mythtv-setup or mythbackend "
                                "to update your database.");
        returnValue = MYTH_SCHEMA_ERROR;
    }
    else   // This client is too old
    {
        if (m_expertMode)
        {
            // Not translated. This string can't appear in released builds.
            message = "Warning: MythTV database has newer"
                      " %1 schema (%2) than expected (%3).";
        }
        else
        {
            message = tr("Error: MythTV database has newer"
                         " %1 schema (%2) than expected (%3).");
            returnValue = MYTH_SCHEMA_ERROR;
        }
    }

    if (m_backupStatus == kDB_Backup_Failed)
        message += "\n" + tr("MythTV was unable to backup your database.");

    if (message.contains("%1"))
        message = message.arg(name, m_DBver, m_newSchemaVer);


    DatabaseParams dbParam = MythDB::getMythDB()->GetDatabaseParams();
    message += "\n\n" + tr("Database Host: %1\nDatabase Name: %2")
                        .arg(dbParam.m_dbHostName, dbParam.m_dbName);

    if (gui)
    {
        if (returnValue == MYTH_SCHEMA_ERROR)
        {
            // Display error, wait for acknowledgement
            // and return warning to caller
            WaitFor(ShowOkPopup(message));
            return MYTH_SCHEMA_ERROR;
        }

        returnValue = GuiPrompt(message, upgradable, m_expertMode);

        if (returnValue == MYTH_SCHEMA_EXIT)
            return MYTH_SCHEMA_EXIT;

        if (m_expertMode)
            return returnValue;

        // The annoying extra confirmation:
        if (m_backupStatus == kDB_Backup_Completed)
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
                      .arg(fileName, dirName);
        }
        else
        {
            message = tr("This cannot be un-done, so having a"
                         " database backup would be a good idea.");
        }
        if (connections)
            message += "\n\n" + warnOtherCl;

        return GuiPrompt(message, upgradable, m_expertMode);
    }

    // We are not in a GUI environment, so try to prompt the user in the shell
    QString resp;

    cout << endl << message.toLocal8Bit().constData() << endl << endl;

    if (returnValue == MYTH_SCHEMA_ERROR)
        return MYTH_SCHEMA_ERROR;

    if (m_backupStatus == kDB_Backup_Failed)
    {
        cout << "WARNING: MythTV was unable to backup your database."
             << endl << endl;
    }
    else if ((m_backupStatus == kDB_Backup_Completed) &&
             (m_backupResult != ""))
    {
        cout << "If your system becomes unstable, "
                "a database backup is located in "
             << m_backupResult.toLocal8Bit().constData() << endl << endl;
    }

    if (m_expertMode)
    {
        resp = getResponse("Would you like to use the existing schema?", "yes");
        if (resp.isEmpty() || resp.startsWith("y", Qt::CaseInsensitive))
            return MYTH_SCHEMA_USE_EXISTING;
    }

    resp = getResponse("\nShall I upgrade this database?", "yes");
    if (!resp.isEmpty() && !resp.startsWith("y", Qt::CaseInsensitive))
        return MYTH_SCHEMA_EXIT;

    if (connections)
        cout << endl << warnOtherCl.toLocal8Bit().constData() << endl;

    if ((m_backupStatus != kDB_Backup_Completed) &&
        (m_backupStatus != kDB_Backup_Empty_DB))
    {
        resp = getResponse("\nA database backup might be a good idea"
                           "\nAre you sure you want to upgrade?", "no");
        if (resp.isEmpty() || resp.startsWith("n", Qt::CaseInsensitive))
            return MYTH_SCHEMA_EXIT;
    }

    return MYTH_SCHEMA_UPGRADE;
}
