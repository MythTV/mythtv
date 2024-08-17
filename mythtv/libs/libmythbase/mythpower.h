#ifndef MYTHPOWER_H
#define MYTHPOWER_H

// Qt
#include <QtGlobal>
#include <QRecursiveMutex>
#include <QObject>
#include <QDateTime>
#include <QTimer>

// MythTV
#include "mythbaseexp.h"
#include "mythchrono.h"
#include "referencecounter.h"

static constexpr std::chrono::seconds DEFAULT_SHUTDOWN_WAIT { 5s };
static constexpr std::chrono::seconds MAXIMUM_SHUTDOWN_WAIT { 30s };

class MBASE_PUBLIC MythPower : public QObject, public ReferenceCounter
{
    Q_OBJECT

  public:
    enum PowerLevel : std::int8_t
    {
        UPS           = -2,
        ACPower       = -1,
        BatteryEmpty  = 0,
        BatteryLow    = 10,
        BatteryFull   = 100,
        UnknownPower  = 101,
        Unset         = 102,
    };

    enum Feature : std::uint8_t
    {
        FeatureNone        = 0x00,
        FeatureShutdown    = 0x01,
        FeatureSuspend     = 0x02,
        FeatureHibernate   = 0x04,
        FeatureRestart     = 0x08,
        FeatureHybridSleep = 0x10
    };

    Q_DECLARE_FLAGS(Features, Feature)

    static MythPower* AcquireRelease(void* Reference, bool Acquire, std::chrono::seconds MinimumDelay = 0s);
    virtual bool RequestFeature    (Feature Request, bool Delay = true);
    Features     GetFeatures       (void);
    bool         IsFeatureSupported(Feature Supported);
    int          GetPowerLevel     (void) const;
    QStringList  GetFeatureList    (void);

  public slots:
    virtual void CancelFeature     (void);

  signals:
    void ShuttingDown   (void);
    void Suspending     (void);
    void Hibernating    (void);
    void Restarting     (void);
    void HybridSleeping (void);
    void WillShutDown   (std::chrono::milliseconds MilliSeconds = 0ms);
    void WillSuspend    (std::chrono::milliseconds MilliSeconds = 0ms);
    void WillHibernate  (std::chrono::milliseconds MilliSeconds = 0ms);
    void WillRestart    (std::chrono::milliseconds MilliSeconds = 0ms);
    void WillHybridSleep(std::chrono::milliseconds MilliSeconds = 0ms);
    void WokeUp         (std::chrono::seconds SecondsAsleep);
    void LowBattery     (void);

  protected slots:
    void FeatureTimeout  (void);
    virtual void Refresh (void) {  }

  protected:
    static QRecursiveMutex s_lock;

    MythPower();
    ~MythPower() override = default;

    virtual void   Init              (void);
    virtual bool   DoFeature         (bool /*Delayed*/ = false) { return false; }
    virtual void   DidWakeUp         (void);
    virtual void   FeatureHappening  (Feature Spontaneous = FeatureNone);
    virtual bool   ScheduleFeature   (enum Feature Type, std::chrono::seconds Delay);
    void           SetRequestedDelay (std::chrono::seconds Delay);
    void           PowerLevelChanged (int Level);
    static QString FeatureToString   (enum Feature Type);
    static bool    FeatureIsEquivalent(Feature First, Feature Second);

    Features  m_features             { FeatureNone };
    Feature   m_scheduledFeature     { FeatureNone };
    bool      m_isSpontaneous        { false };
    std::chrono::seconds  m_maxRequestedDelay    { 0s };
    std::chrono::seconds  m_maxSupportedDelay    { MAXIMUM_SHUTDOWN_WAIT };
    QTimer    m_featureTimer;
    QDateTime m_sleepTime;
    int       m_powerLevel           { Unset };
    bool      m_warnForLowBattery    { false };

  private:
    Q_DISABLE_COPY(MythPower)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MythPower::Features)

#endif // MYTHPOWER_H
