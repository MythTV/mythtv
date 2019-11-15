#ifndef BACKENDHOUSEKEEPER_H_
#define BACKENDHOUSEKEEPER_H_

#include "housekeeper.h"
#include "mythsystemlegacy.h"

class LogCleanerTask : public DailyHouseKeeperTask
{
  public:
    LogCleanerTask(void) : DailyHouseKeeperTask("LogClean", kHKGlobal) {};
    bool DoRun(void) override; // HouseKeeperTask
};


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
    virtual ~RadioStreamUpdateTask(void);
    bool DoRun(void) override; // HouseKeeperTask
    bool DoCheckRun(QDateTime now) override; // PeriodicHouseKeeperTask
    void Terminate(void) override; // HouseKeeperTask
  private:
    MythSystemLegacy *m_msMU;
};

class ThemeUpdateTask : public DailyHouseKeeperTask
{
  public:
    ThemeUpdateTask(void) : DailyHouseKeeperTask("ThemeUpdateNotifications",
                                            kHKGlobal, kHKRunOnStartup),
                            m_running(false) {};
    bool DoRun(void) override; // HouseKeeperTask
    bool DoCheckRun(QDateTime now) override; // PeriodicHouseKeeperTask
    void Terminate(void) override; // HouseKeeperTask
  private:
    bool LoadVersion(const QString &version, int download_log_level);

    bool m_running;
    QString m_url;
};

class ArtworkTask : public DailyHouseKeeperTask
{
  public:
    ArtworkTask(void);
    virtual ~ArtworkTask(void);
    bool DoRun(void) override; // HouseKeeperTask
    bool DoCheckRun(QDateTime now) override; // PeriodicHouseKeeperTask
    void Terminate(void) override; // HouseKeeperTask
  private:
    MythSystemLegacy *m_msMML;
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
    virtual ~MythFillDatabaseTask(void);

    static bool UseSuggestedTime(void);

    bool DoCheckRun(QDateTime now) override; // PeriodicHouseKeeperTask
    bool DoRun(void) override; // HouseKeeperTask

    void Terminate(void) override; // HouseKeeperTask

    void SetHourWindowFromDB(void);
  private:
    MythSystemLegacy *m_msMFD;
//    bool m_running;
};



#endif
