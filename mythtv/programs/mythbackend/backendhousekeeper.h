#ifndef BACKENDHOUSEKEEPER_H_
#define BACKENDHOUSEKEEPER_H_

#include "housekeeper.h"
#include "mythsystemlegacy.h"

class LogCleanerTask : public DailyHouseKeeperTask
{
  public:
    LogCleanerTask(void) : DailyHouseKeeperTask("LogClean", kHKGlobal) {};
    bool DoRun(void);
};


class CleanupTask : public DailyHouseKeeperTask
{
  public:
    CleanupTask(void) : DailyHouseKeeperTask("DBCleanup", kHKGlobal) {};
    bool DoRun(void);

  private:
    void CleanupOldRecordings(void);
    void CleanupInUsePrograms(void);
    void CleanupOrphanedLiveTV(void);
    void CleanupRecordedTables(void);
    void CleanupProgramListings(void);
};


class ThemeUpdateTask : public DailyHouseKeeperTask
{
  public:
    ThemeUpdateTask(void) : DailyHouseKeeperTask("ThemeUpdateNotifications",
                                            kHKGlobal, kHKRunOnStartup) {};
    bool DoRun(void);
};


class ArtworkTask : public DailyHouseKeeperTask
{
  public:
    ArtworkTask(void) : DailyHouseKeeperTask("RecordedArtworkUpdate",
                                             kHKGlobal, kHKRunOnStartup) {};
    bool DoRun(void);
};


class JobQueueRecoverTask : public DailyHouseKeeperTask
{
  public:
    JobQueueRecoverTask(void) : DailyHouseKeeperTask("JobQueueRecover",
                                                     kHKLocal) {};
    bool DoRun(void);
};


class MythFillDatabaseTask : public DailyHouseKeeperTask
{
  public:
    MythFillDatabaseTask(void);

    static bool UseSuggestedTime(void);

    virtual bool DoCheckRun(QDateTime now);
    virtual bool DoRun(void);

    virtual void Terminate(void);

    void SetHourWindow(void);
  private:
    MythSystemLegacy *m_msMFD;
//    bool m_running;
};



#endif
