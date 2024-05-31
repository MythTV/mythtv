
#ifndef SCHEMA_WIZARD_H
#define SCHEMA_WIZARD_H

#include <QObject>
#include <QString>

#include "libmythui/mythuiexp.h"
#include "libmythbase/dbutil.h"

class MythUIBusyDialog;

/// Return values from PromptForUpgrade()
enum MythSchemaUpgrade : std::uint8_t
{
    MYTH_SCHEMA_EXIT         = 1,
    MYTH_SCHEMA_ERROR        = 2,
    MYTH_SCHEMA_UPGRADE      = 3,
    MYTH_SCHEMA_USE_EXISTING = 4
};

/** \brief Provides UI and helper functions for DB Schema updates.
 *  See dbcheck.cpp's UpgradeTVDatabaseSchema() for usage.
 */
class MUI_PUBLIC SchemaUpgradeWizard : public QObject, public DBUtil
{
    Q_OBJECT

  public:
    SchemaUpgradeWizard(QString DBSchemaSetting, QString appName,
                        QString upgradeSchemaVal);
    ~SchemaUpgradeWizard() override;


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
                                            bool upgradeAllowed,
                                            bool upgradeIfNoUI,
                                            int  minDBMSmajor = 0,
                                            int  minDBMSminor = 0,
                                            int  minDBMSpoint = 0);

    QString m_DBver;               ///< Schema version in the database
    bool    m_emptyDB {false};     ///< Is the database currently empty?
    int     m_versionsBehind {-1}; ///< How many schema versions old is the DB?

    MythDBBackupStatus m_backupStatus{kDB_Backup_Unknown}; ///< BackupDB() status

  private:
    void              BusyPopup(const QString &message);
    static MythSchemaUpgrade GuiPrompt(const QString &message,
                                bool upgradable, bool expert);

    bool              m_autoUpgrade {false};///< If no UI, always upgrade
    QString           m_backupResult;       ///< File path, or __FAILED__
    MythUIBusyDialog *m_busyPopup {nullptr};///< Displayed during long pauses
    bool              m_expertMode {false}; ///< Also allow newer DB schema
    QString           m_schemaSetting;      ///< To lookup the schema version
    QString           m_schemaName;         ///< Shown to user in logs
    QString           m_newSchemaVer;       ///< What we need to upgrade to

};

#endif // SCHEMA_WIZARD_H
