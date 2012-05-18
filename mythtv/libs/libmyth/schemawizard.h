
#ifndef SCHEMA_WIZARD_H
#define SCHEMA_WIZARD_H

#include <QObject>
#include <QString>

#include "mythexp.h"
#include "dbutil.h"
#include "mythmainwindow.h"
#include "mythprogressdialog.h"

/// Return values from PromptForUpgrade()
enum MythSchemaUpgrade
{
    MYTH_SCHEMA_EXIT         = 1,
    MYTH_SCHEMA_ERROR        = 2,
    MYTH_SCHEMA_UPGRADE      = 3,
    MYTH_SCHEMA_USE_EXISTING = 4
};

/** \brief Provides UI and helper functions for DB Schema updates.
 *  See dbcheck.cpp's UpgradeTVDatabaseSchema() for usage.
 */
class MPUBLIC SchemaUpgradeWizard : public QObject, public DBUtil
{
    Q_OBJECT

  public:
    SchemaUpgradeWizard(const QString &DBSchemaSetting, const QString &appName,
                        const QString &upgradeSchemaVal);
    ~SchemaUpgradeWizard();


    /// Call DBUtil::BackupDB(), and store results
    MythDBBackupStatus BackupDB(void);

    /// How many schema versions old is the DB?
    int Compare(void);

    /// Instead of creating a new wizard, use the existing one
    /// for its DB backup file & results and expert settings.
    static SchemaUpgradeWizard *Get(const QString &DBSchemaSetting,
                                    const QString &appName,
                                    const QString &upgradeSchemaVal);

    /// Query user, to prevent silent, automatic database upgrades
    enum MythSchemaUpgrade PromptForUpgrade(const char *name,
                                            const bool upgradeAllowed,
                                            const bool upgradeIfNoUI,
                                            const int  minDMBSmajor = 0,
                                            const int  minDBMSminor = 0,
                                            const int  minDBMSpoint = 0);

    QString DBver;            ///< Schema version in the database
    bool    emptyDB;          ///< Is the database currently empty?
    int     versionsBehind;   ///< How many schema versions old is the DB?

    MythDBBackupStatus backupStatus;   ///< BackupDB() status

  private:
    void              BusyPopup(const QString &message);
    MythSchemaUpgrade GuiPrompt(const QString &message,
                                bool upgradable, bool expert);

    bool              m_autoUpgrade;        ///< If no UI, always upgrade
    QString           m_backupResult;       ///< File path, or __FAILED__
    MythUIBusyDialog *m_busyPopup;          ///< Displayed during long pauses
    bool              m_expertMode;         ///< Also allow newer DB schema
    QString           m_schemaSetting;      ///< To lookup the schema version
    QString           m_schemaName;         ///< Shown to user in logs
    QString           m_newSchemaVer;       ///< What we need to upgrade to

};

#endif // SCHEMA_WIZARD_H
