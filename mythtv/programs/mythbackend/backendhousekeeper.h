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

class FindEncoders : public PeriodicHouseKeeperTask
{
  public:
    FindEncoders(void) :
        PeriodicHouseKeeperTask("FindEncoders", kINTERVAL1,
                                kINTERVAL_EARLY_PCT, kINTERVAL_LATE_PCT) {}

    bool DoRun(void) override; // HouseKeeperTask

  private:
    // Start running every minute, and then backoff every fifth run
    // per the interval times below.
    static constexpr std::chrono::minutes kINTERVAL1 {  1min };
    static constexpr std::chrono::minutes kINTERVAL2 {  2min };
    static constexpr std::chrono::minutes kINTERVAL3 {  5min };
    static constexpr std::chrono::minutes kINTERVAL4 { 15min };
    static constexpr std::chrono::minutes kINTERVAL5 { 60min };
    static constexpr float kINTERVAL_EARLY_PCT { 0.95 };
    static constexpr float kINTERVAL_LATE_PCT  { 1.05 };
    int  m_called { 0 };
};

#endif
