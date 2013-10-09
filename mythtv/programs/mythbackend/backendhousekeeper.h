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
                                            kHKGlobal, kHKRunOnStartup),
                            m_running(false) {};
    bool DoRun(void);
    virtual void Terminate(void);
  private:
    bool m_running;
    QString m_url;
};


class ArtworkTask : public DailyHouseKeeperTask
{
  public:
    ArtworkTask(void);
    virtual ~ArtworkTask(void);
    bool DoRun(void);
    virtual bool DoCheckRun(QDateTime now);
    virtual void Terminate(void);
  private:
    MythSystemLegacy *m_msMML;
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
    virtual ~MythFillDatabaseTask(void);

    static bool UseSuggestedTime(void);

    virtual bool DoCheckRun(QDateTime now);
    virtual bool DoRun(void);

    virtual void Terminate(void);

    void SetHourWindowFromDB(void);
  private:
    MythSystemLegacy *m_msMFD;
//    bool m_running;
};



#endif
