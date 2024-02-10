// MythTV
#include "mythcorecontext.h"
#include "mythpower.h"

#ifdef USING_DBUS
#include "platforms/mythpowerdbus.h"
#endif

#ifdef Q_OS_DARWIN
#include "platforms/mythpowerosx.h"
#endif

#define LOC QString("Power: ")

QRecursiveMutex MythPower::s_lock;

/*! \class MythPower
 *
 * MythPower is a reference counted, singleton class.
 *
 * Classes wishing to listen for or schedule power events should acquire a reference
 * to MythPower via AcquireRelease using a suitable Reference and Acquire set to true.
 *
 * AcquireRelease will always return a valid pointer but the default implementation
 * has no actual power support.
 *
 * If the class in question wishes to register a delay so that it can safely
 * cleanup before shutdown/suspend, it should pass a MinimumDelay (seconds) to
 * AcquireRelease. The MythPower object will then attempt to delay any events
 * for at least that period of time. This behaviour is not guaranteed - most
 * notably in the case of externally triggered events. The actual delay attempted
 * will be the maximum of those registered and, in the case of user initiated
 * events, the EXIT_SHUTDOWN_DELAY user setting (which defaults to 3 seconds).
 *
 * There is no method to subsequently alter the delay. Should this be needed, release
 * the current reference and re-acquire with a new MinimumDelay. To ensure the
 * underlying MythPower object is not deleted, take a temporary reference with
 * AcquireRelease using a different, unique Reference value.
 *
 * Do not acquire multiple references with the same Reference value. This will lead
 * to unexpected delay behaviour.
 *
 * To release the MythPower reference, call AcquireRelease with the same Reference
 * (typically a pointer to the owning class) and Acquire set to false.
 *
 * To check supported power behaviours, call either GetFeatures or IsFeatureSupported.
 *
 * Call RequestFeature to initiate shutdown/suspend etc. The default behaviour is
 * to delay the action.
 *
 * Call CancelFeature to cancel a previously scheduled shutdown/suspend.
 * \note CancelFeature is currently untested and may be removed. It is unclear
 * whether it is useable. As soon as the feature is requested, various system
 * processes start to shutdown. This often renders the GUI either unresponsive or
 * simply not visible. Hence trying to show a cancel dialog is pointless.
 *
 * Signals
 *
 * WillShutdown etc are emitted in advance and provide the listener with an
 * opportunity to do stuff. There is no guarantee on how long they have.
 * ShuttingDown etc are emitted when the requested feature (or system event) is
 * imminent. There is, in theory, no time to do stuff.
 * WokeUp is emitted when the system has been woken up OR when a scheduled event
 * has been cancelled.
 *
 * \todo Add OSX subclass
 * \todo Add Windows subclass
 * \todo Add Android subclass
 * \todo Add sending Myth events as well as emitting signals.
*/
MythPower* MythPower::AcquireRelease(void *Reference, bool Acquire, std::chrono::seconds MinimumDelay)
{
    static MythPower* s_instance = nullptr;
    static QHash<void*,std::chrono::seconds> s_delays;

    QMutexLocker locker(&s_lock);
    if (Acquire)
    {
        // Upref or create
        if (s_instance)
        {
            s_instance->IncrRef();
        }
        else
        {
#ifdef Q_OS_DARWIN
            // NB OSX may have DBUS but it won't help here
            s_instance = new MythPowerOSX();
#elif defined(USING_DBUS)
            if (MythPowerDBus::IsAvailable())
                s_instance = new MythPowerDBus();
#endif
            // default no functionality
            if (!s_instance)
                s_instance = new MythPower();
        }
        // Append additional feature delay request
        s_delays.insert(Reference, MinimumDelay);
    }
    else
    {
        // Remove requested delay
        s_delays.remove(Reference);

        // Decref and ensure nulled on last use
        if (s_instance)
            if (s_instance->DecrRef() == 0)
                s_instance = nullptr;
    }

    if (s_instance)
    {
        // Update the maximum requested delay
        std::chrono::seconds max = std::max_element(s_delays.cbegin(), s_delays.cend()).value();
        s_instance->SetRequestedDelay(max);
    }
    return s_instance;
}

MythPower::MythPower()
  : ReferenceCounter("Power")
{
    m_featureTimer.setSingleShot(true);
    connect(&m_featureTimer, &QTimer::timeout, this, &MythPower::FeatureTimeout);
}

void MythPower::Init(void)
{
    QStringList supported = GetFeatureList();
    if (supported.isEmpty())
        supported << "None";
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Supported actions: %1").arg(supported.join(",")));
}

void MythPower::SetRequestedDelay(std::chrono::seconds Delay)
{
    m_maxRequestedDelay = Delay;
}

MythPower::Features MythPower::GetFeatures(void)
{
    Features result = FeatureNone;
    s_lock.lock();
    result = m_features;
    s_lock.unlock();
    return result;
}

QStringList MythPower::GetFeatureList(void)
{
    QStringList supported;
    MythPower::Features features = GetFeatures();
    if (features.testFlag(FeatureSuspend))     supported << FeatureToString(FeatureSuspend);
    if (features.testFlag(FeatureHibernate))   supported << FeatureToString(FeatureHibernate);
    if (features.testFlag(FeatureRestart))     supported << FeatureToString(FeatureRestart);
    if (features.testFlag(FeatureShutdown))    supported << FeatureToString(FeatureShutdown);
    if (features.testFlag(FeatureHybridSleep)) supported << FeatureToString(FeatureHybridSleep);
    return supported;
}

bool MythPower::IsFeatureSupported(Feature Supported)
{
    bool result = false;
    s_lock.lock();
    result = ((m_features & Supported) != 0U);
    s_lock.unlock();
    return result;
}

int MythPower::GetPowerLevel(void) const
{
    int result = UnknownPower;
    s_lock.lock();
    result = m_powerLevel;
    s_lock.unlock();
    return result;
}

bool MythPower::RequestFeature(Feature Request, bool Delay)
{
    QMutexLocker locker(&s_lock);

    if ((Request == FeatureNone) || !(m_features & Request))
        return false;

    // N.B Always check for a new user delay value as this class is persistent.
    // Default is user preference, limited by the maximum supported system value
    // and possibly overriden by the maximum delay requested by other Myth classes.
    auto user = gCoreContext->GetDurSetting<std::chrono::seconds>("EXIT_SHUTDOWN_DELAY", 3s);
    auto delay = std::clamp(user, m_maxRequestedDelay, m_maxSupportedDelay);

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Delay: %1 User: %2 Requested: %3 Supported: %4")
        .arg(Delay).arg(user.count()).arg(m_maxRequestedDelay.count())
        .arg(m_maxSupportedDelay.count()));

    if (!Delay || delay < 1s)
    {
        m_scheduledFeature = Request;
        DoFeature();
    }

    if (!ScheduleFeature(Request, delay))
        return false;

    auto remaining = m_featureTimer.remainingTimeAsDuration();
    switch (Request)
    {
        case FeatureSuspend:     emit WillSuspend(remaining);     break;
        case FeatureShutdown:    emit WillShutDown(remaining);    break;
        case FeatureRestart:     emit WillRestart(remaining);     break;
        case FeatureHibernate:   emit WillHibernate(remaining);   break;
        case FeatureHybridSleep: emit WillHybridSleep(remaining); break;
        default: break;
    }
    return true;
}

/// This is untested as it is currently not clear whether it is useful.
void MythPower::CancelFeature(void)
{
    QMutexLocker locker(&s_lock);

    if (!m_featureTimer.isActive() || !m_scheduledFeature)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "No power request to cancel");
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Cancelling %1 request with %2 seconds remaining")
            .arg(FeatureToString(m_scheduledFeature)).arg(m_featureTimer.remainingTime() / 1000));
        DidWakeUp();
    }
    m_scheduledFeature = FeatureNone;
    m_featureTimer.stop();
}

void MythPower::FeatureTimeout(void)
{
    DoFeature();
}

QString MythPower::FeatureToString(enum Feature Type)
{
    switch (Type)
    {
        case FeatureRestart:     return tr("Restart");
        case FeatureSuspend:     return tr("Suspend");
        case FeatureShutdown:    return tr("Shutdown");
        case FeatureHibernate:   return tr("Hibernate");
        case FeatureHybridSleep: return tr("HybridSleep");
        case FeatureNone:     break;
    }
    return "Unknown";
}

bool MythPower::FeatureIsEquivalent(Feature First, Feature Second)
{
    bool suspend  = Second == FeatureSuspend || Second == FeatureHibernate ||
                    Second == FeatureHybridSleep;
    bool poweroff = Second == FeatureRestart || Second == FeatureShutdown;
    switch (First)
    {
        case FeatureNone: return Second == FeatureNone;
        case FeatureSuspend:
        case FeatureHibernate:
        case FeatureHybridSleep: return suspend;
        case FeatureRestart:
        case FeatureShutdown: return poweroff;
    }
    return false;
}

bool MythPower::ScheduleFeature(enum Feature Type, std::chrono::seconds Delay)
{
    if (Type == FeatureNone || Delay > MAXIMUM_SHUTDOWN_WAIT)
        return false;

    if (m_featureTimer.isActive() || m_scheduledFeature)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Ignoring %1 request: %2 pending in %3 seconds")
            .arg(FeatureToString(Type), FeatureToString(m_scheduledFeature))
            .arg(m_featureTimer.remainingTime() / 1000));
        return false;
    }

    m_scheduledFeature = Type;
    m_featureTimer.start(Delay);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Will %1 in %2 seconds")
        .arg(FeatureToString(Type)).arg(Delay.count()));
    return true;
}

/*! \brief Signal to the rest of MythTV that the given feature will happen now.
 *
 * \note If other elements of MythTV have not previously been notified of a
 * pending action there is little they can do now as there is no guarantee anything
 * will be completed before the system goes offline.
*/
void MythPower::FeatureHappening(Feature Spontaneous)
{
    if (FeatureNone != Spontaneous)
    {
        m_scheduledFeature = Spontaneous;
        m_isSpontaneous = true;
    }

    if (!m_scheduledFeature)
        return;

    m_sleepTime = QDateTime::currentDateTime();
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("About to: %1 %2")
        .arg(FeatureToString(m_scheduledFeature),
             m_isSpontaneous ? QString("(System notification)") : ""));

    switch (m_scheduledFeature)
    {
        case FeatureSuspend:     emit Suspending();   break;
        case FeatureShutdown:    emit ShuttingDown(); break;
        case FeatureRestart:     emit Restarting();   break;
        case FeatureHibernate:   emit Hibernating();  break;
        case FeatureHybridSleep: emit HybridSleeping(); break;
        case FeatureNone:     break;
    }
}

void MythPower::DidWakeUp(void)
{
    QMutexLocker locker(&s_lock);

    m_scheduledFeature = FeatureNone;
    m_isSpontaneous = false;
    m_featureTimer.stop();
    static constexpr qint64 kSecsInDay { 24LL * 60 * 60 };
    QDateTime now = QDateTime::currentDateTime();
    qint64 secs  = m_sleepTime.secsTo(now);
    qint64 days  = secs / kSecsInDay;
    QTime time = QTime(0, 0).addSecs(secs % kSecsInDay);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Woke up after %1days %2hours %3minutes and %4seconds")
        .arg(days).arg(time.hour()).arg(time.minute()).arg(time.second()));
    emit WokeUp(std::chrono::seconds(m_sleepTime.secsTo(now)));
}

void MythPower::PowerLevelChanged(int Level)
{
    if (Level == m_powerLevel)
        return;

    if (Level == ACPower)
    {
        m_warnForLowBattery = true;
        LOG(VB_GENERAL, LOG_INFO, LOC + "On AC power");
    }
    else if (Level == UPS)
    {
        m_warnForLowBattery = true;
        LOG(VB_GENERAL, LOG_INFO, LOC + "On UPS");
    }
    else if (Level < UnknownPower)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Battery at %1%").arg(Level));
        if (Level <= BatteryLow)
        {
            if (m_warnForLowBattery)
            {
                m_warnForLowBattery = false;
                LOG(VB_GENERAL, LOG_INFO, LOC + "Low battery!");
                emit LowBattery();
            }
        }
        else
        {
            m_warnForLowBattery = true;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Unknown power source");
    }

    m_powerLevel = Level;
}
