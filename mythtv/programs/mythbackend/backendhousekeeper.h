#ifndef BACKENDHOUSEKEEPER_H_
#define BACKENDHOUSEKEEPER_H_

#include "libmythbase/housekeeper.h"
#include "libmythbase/mythsystemlegacy.h"

class CleanupTask : public DailyHouseKeeperTask
{
  public:
    CleanupTask(void) : DailyHouseKeeperTask("DBCleanup", kHKGlobal) {};
    bool DoRun(void) override; // HouseKeeperTask

  private:
    static void CleanupOldRecordings(void);
    static void CleanupInUsePrograms(void);
    static void CleanupOrphanedLiveTV(void);
    static void CleanupRecordedTables(void);
    static void CleanupChannelTables(void);
    static void CleanupProgramListings(void);
};

class RadioStreamUpdateTask : public DailyHouseKeeperTask
{
  public:
    RadioStreamUpdateTask(void);
    ~RadioStreamUpdateTask(void) override;
    bool DoRun(void) override; // HouseKeeperTask
    bool DoCheckRun(const QDateTime& now) override; // PeriodicHouseKeeperTask
    void Terminate(void) override; // HouseKeeperTask
  private:
    MythSystemLegacy *m_msMU { nullptr };
};

class ThemeUpdateTask : public DailyHouseKeeperTask
{
  public:
    ThemeUpdateTask(void) : DailyHouseKeeperTask("ThemeUpdateNotifications",
                                            kHKGlobal, kHKRunOnStartup) {};
    bool DoRun(void) override; // HouseKeeperTask
    bool DoCheckRun(const QDateTime& now) override; // PeriodicHouseKeeperTask
    void Terminate(void) override; // HouseKeeperTask
  private:
    bool LoadVersion(const QString &version, int download_log_level);

    bool m_running { false };
    QString m_url;
};

class ArtworkTask : public DailyHouseKeeperTask
{
  public:
    ArtworkTask(void);
    ~ArtworkTask(void) override;
    bool DoRun(void) override; // HouseKeeperTask
    bool DoCheckRun(const QDateTime& now) override; // PeriodicHouseKeeperTask
    void Terminate(void) override; // HouseKeeperTask
  private:
    MythSystemLegacy *m_msMML { nullptr };
};


class JobQueueRecoverTask : public DailyHouseKeeperTask
{
  public:
    JobQueueRecoverTask(void) : DailyHouseKeeperTask("JobQueueRecover",
                                                     kHKLocal) {};
    bool DoRun(void) override; // HouseKeeperTask
};


class MythFillDatabaseTask : public DailyHouseKeeperTask
{
  public:
    MythFillDatabaseTask(void);
    ~MythFillDatabaseTask(void) override;

    static bool UseSuggestedTime(void);

    bool DoCheckRun(const QDateTime& now) override; // PeriodicHouseKeeperTask
    bool DoRun(void) override; // HouseKeeperTask

    void Terminate(void) override; // HouseKeeperTask

    void SetHourWindowFromDB(void);
  private:
    MythSystemLegacy *m_msMFD { nullptr };
//    bool m_running;
};



#endif
