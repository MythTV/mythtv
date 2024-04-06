// MythTV
#include "mythlogging.h"
#include "mythpowerdbus.h"

// Std
#include <unistd.h>
#include <algorithm>

// Qt
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>

#define LOC QString("PowerDBus: ")

#define FREE_SERVICE     (QString("org.freedesktop."))
#define FREE_PATH        (QString("/org/freedesktop/"))
#define UPOWER           (QString("UPower"))
#define LOGIN1           (QString("login1"))
#define UPOWER_SERVICE   (FREE_SERVICE + UPOWER)
#define UPOWER_PATH      (FREE_PATH + UPOWER)
#define UPOWER_INTERFACE (UPOWER_SERVICE)
#define LOGIN1_SERVICE   (FREE_SERVICE + LOGIN1)
#define LOGIN1_PATH      (FREE_PATH + LOGIN1)
#define LOGIN1_INTERFACE (LOGIN1_SERVICE + QString(".Manager"))

/*! \brief Static check for DBus interfaces that support some form of power management.
 *
 * This currently looks for UPower and login1 (logind) interfaces.
 *
 * UPower is used for battery status.
 *
 * logind manages shutdown, suspend etc. We make use of ScheduleShutdown where
 * possible to integrate better with other services and mimic ScheduleShutdown
 * otherwise (e.g. for suspend). Two different MythTV services (e.g both frontend
 * and backend) should integrate well with their behaviour as both can delay
 * events independantly as needed (though this might lead to user confusion if
 * an event is delayed longer than they expect).
 *
 * \note ConsoleKit appears to be dead in the water (and reported as dangerous!).
 * \note The UPower interface is now focused on battery device status and does
 * not support Suspend or Hibernate in more recent specs.
 * \note HAL is deprecated.
 *
 * \todo Check status of KDE ("org.kde.KSMServerInterface") and Gnome("org.gnome.SessionManager").
*/
bool MythPowerDBus::IsAvailable(void)
{
    QMutexLocker locker(&s_lock);
    static bool s_available = false;
    static bool s_checked   = false;
    if (!s_checked)
    {
        s_checked = true;
        auto* upower = new QDBusInterface(
            UPOWER_SERVICE, UPOWER_PATH, UPOWER_INTERFACE, QDBusConnection::systemBus());
        auto* login1 = new QDBusInterface(
            LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_INTERFACE, QDBusConnection::systemBus());
        s_available = upower->isValid() || login1->isValid();
        delete upower;
        delete login1;
    }
    return s_available;
}

MythPowerDBus::MythPowerDBus()
{
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &MythPowerDBus::ReleaseLock);
    MythPowerDBus::Init();
}

MythPowerDBus::~MythPowerDBus()
{
    m_scheduledFeature = FeatureNone;
    ReleaseLock();
    if (m_upowerInterface || m_logindInterface)
        LOG(VB_GENERAL, LOG_INFO, LOC + "Closing interfaces");
    delete m_upowerInterface;
    delete m_logindInterface;
}

void MythPowerDBus::Init(void)
{
    // create interfaces
    m_upowerInterface = new QDBusInterface(UPOWER_SERVICE, UPOWER_PATH, UPOWER_INTERFACE, m_bus);
    m_logindInterface = new QDBusInterface(LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_INTERFACE, m_bus);

    if (m_upowerInterface)
    {
        if (!m_upowerInterface->isValid())
        {
            delete m_upowerInterface;
            m_upowerInterface = nullptr;
        }
    }

    if (m_logindInterface)
    {
        if (!m_logindInterface->isValid())
        {
            delete m_logindInterface;
            m_logindInterface = nullptr;
        }
    }

    if (!m_upowerInterface)
        LOG(VB_GENERAL, LOG_ERR, LOC + "No UPower interface. Unable to monitor battery state");
    if (!m_logindInterface)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "No login1 interface. Cannot change system power state");

    // listen for sleep/wake events
    if (m_logindInterface)
    {
        // delay system requests
        AcquireLock(FeatureSuspend | FeatureShutdown);

        if (!m_bus.connect(LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_INTERFACE, "PrepareForSleep",
                      this, SLOT(DBusSuspending(bool))))
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to listen for sleep events");
        }
        if (!m_bus.connect(LOGIN1_SERVICE, LOGIN1_PATH, LOGIN1_INTERFACE, "PrepareForShutdown",
                      this, SLOT(DBusShuttingDown(bool))))
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Failed to listen for shutdown events");
        }
    }

    // populate power devices (i.e. batteries)
    if (m_upowerInterface)
    {
        QDBusReply<QList<QDBusObjectPath> > response =
            m_upowerInterface->call(QLatin1String("EnumerateDevices"));
        if (response.isValid())
        {
            QList devices = response.value();
            for (const auto& device : std::as_const(devices))
                DeviceAdded(device);
        }

        if (!m_bus.connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_SERVICE, "Changed", this, SLOT(Changed())))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to register for Changed");
        }

        if (!m_bus.connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_SERVICE, "DeviceChanged", "o",
                           this, SLOT(DeviceChanged(QDBusObjectPath))))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceChanged");
        }

        if (!m_bus.connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_SERVICE, "DeviceAdded", "o",
                           this, SLOT(DeviceAdded(QDBusObjectPath))))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceAdded");
        }

        if (!m_bus.connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_SERVICE, "DeviceRemoved", "o",
                           this, SLOT(DeviceRemoved(QDBusObjectPath))))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceRemoved");
        }
    }

    Changed();
    MythPower::Init();
}

bool MythPowerDBus::DoFeature(bool Delayed)
{
    if (!m_logindInterface ||
          ((m_features & m_scheduledFeature) == 0U) ||
          (m_scheduledFeature == 0U))
        return false;

    if (!Delayed)
        ReleaseLock();
    switch (m_scheduledFeature)
    {
        case FeatureSuspend:     m_logindInterface->call("Suspend", false);     break;
        case FeatureShutdown:    m_logindInterface->call("PowerOff", false);    break;
        case FeatureHibernate:   m_logindInterface->call("Hibernate", false);   break;
        case FeatureRestart:     m_logindInterface->call("Reboot", false);      break;
        case FeatureHybridSleep: m_logindInterface->call("HybridSleep", false); break;
        case FeatureNone: return false;
    }
    return true;
}

void MythPowerDBus::DBusSuspending(bool Stopping)
{
    if (Stopping)
    {
        if (FeatureIsEquivalent(m_scheduledFeature, FeatureSuspend))
            return;

        if (UpdateStatus())
            return;

        FeatureHappening(FeatureSuspend);
        return;
    }
    DidWakeUp();
}

void MythPowerDBus::DBusShuttingDown(bool Stopping)
{
    if (Stopping)
    {
        if (FeatureIsEquivalent(m_scheduledFeature, FeatureShutdown))
            return;

        if (UpdateStatus())
            return;

        FeatureHappening(FeatureShutdown);
        return;
    }
    DidWakeUp(); // after hibernate?
}

bool MythPowerDBus::UpdateStatus(void)
{
    if (!m_logindInterface || m_scheduledFeature)
        return false;

    Feature feature = FeatureNone;
    QVariant property = m_logindInterface->property("PreparingForShutdown");
    if (property.isValid() && property.toBool())
        feature = FeatureShutdown;

    if (!feature)
    {
        property = m_logindInterface->property("PreparingForSleep");
        if (property.isValid() && property.toBool())
            feature = FeatureSuspend;
    }

    if (!feature)
        return false;

    m_scheduledFeature = feature;

    // TODO It would be nice to check the ScheduledShutdown property to confirm
    // the time available before shutdown/suspend but Qt doesn't like the type
    // definition and aborts. Requires custom handling that is beyond the wit of this man.
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("System will %1").arg(FeatureToString(feature)));

    // Attempt to delay the action.

    // NB we don't care about user preference here. We are giving
    // MythTV interested components an opportunity to cleanup before
    // an externally initiated shutdown/suspend
    auto delay = std::clamp(m_maxRequestedDelay, 0s, m_maxSupportedDelay);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Trying to delay system %1 for %2 seconds")
        .arg(FeatureToString(feature)).arg(delay.count()));
    m_delayTimer.start(delay);

    switch (feature)
    {
        case FeatureSuspend:  emit WillSuspend(delay);  break;
        case FeatureShutdown: emit WillShutDown(delay); break;
        default: break;
    }

    return true;
}

void MythPowerDBus::DidWakeUp(void)
{
    QMutexLocker locker(&s_lock);
    m_delayTimer.stop();
    MythPower::DidWakeUp();
    AcquireLock(FeatureSuspend | FeatureShutdown);
}

/*! \brief Schedule a MythTV initiated power feature.
 *
 * The default MythPopwer implementation will schedule the feature at a point in
 * the future. logind however allows us to schedule shutdown events (but not
 * suspend/hibernate etc). This has the advantage that logind will send a
 * PrepareForShutdown signal - which gives other services advanced warning of
 * the pending shutdown and allows them to prepare properly.
 *
 * For suspend type events, we mimic the delay by initiating the suspend straight
 * away but retaining the inhibition lock until we are ready. This plays nicely
 * with other services (which receive the PrepareForSleep signal immediately) but
 * on some systems means the display is turned off too soon.
*/
bool MythPowerDBus::ScheduleFeature(enum Feature Type, std::chrono::seconds Delay)
{
    if (!MythPower::ScheduleFeature(Type, Delay))
        return false;

    if (Delay < 1s)
        return true;

    // try and use ScheduleShutdown as it gives the system the opportunity
    // to inhibit shutdown and just plays nicely with other users - not least
    // any mythbackend that is running. Suspend/hibernate are not supported.
    if (m_logindInterface && (Type == FeatureShutdown || Type == FeatureRestart))
    {
        auto time = nowAsDuration<std::chrono::milliseconds>();
        if (time > 0ms)
        {
            std::chrono::milliseconds millisecs = time + Delay;
            QLatin1String type;
            switch (Type)
            {
                case FeatureShutdown: type = QLatin1String("poweroff"); break;
                case FeatureRestart:  type = QLatin1String("reboot");   break;
                default: break;
            }
            QDBusReply<void> reply =
                m_logindInterface->call(QLatin1String("ScheduleShutdown"), type,
                                        static_cast<qint64>(millisecs.count()));

            if (reply.isValid() && !reply.error().isValid())
            {
                // cancel the default handling.
                m_featureTimer.stop();
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 scheduled via logind")
                    .arg(FeatureToString(Type)));
                m_delayTimer.start(Delay);
            }
            else
            {
                LOG(VB_GENERAL, LOG_DEBUG, LOC +
                    QString("Failed to schedule %1 - falling back to default behaviour")
                    .arg(FeatureToString(Type)));
                LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Error %1 Message %2")
                    .arg(reply.error().name(), reply.error().message()));
            }
        }
    }
    else if (Type == FeatureSuspend)
    {
        // no logind scheduling but intiate suspend now and retain lock until ready
        m_featureTimer.stop();
        m_delayTimer.start(Delay);
        DoFeature(true);
    }

    return true;
}

/// This is untested
void MythPowerDBus::CancelFeature(void)
{
    QMutexLocker locker(&s_lock);

    if (m_delayTimer.isActive())
        m_delayTimer.stop();
    MythPower::CancelFeature();
}

void MythPowerDBus::Changed(void)
{
    QMutexLocker locker(&s_lock);
    UpdateProperties();
    UpdateBattery();
}

void MythPowerDBus::DeviceAdded(const QDBusObjectPath& Device)
{
    {
        QMutexLocker locker(&s_lock);
        if (m_batteries.contains(Device.path()))
            return;
        m_batteries.insert(Device.path(), RetrieveBatteryLevel(Device.path()));
    }
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Added UPower.Device '%1'").arg(Device.path()));
    UpdateBattery();
}

void MythPowerDBus::DeviceRemoved(const QDBusObjectPath& Device)
{
    {
        QMutexLocker locker(&s_lock);
        if (!m_batteries.contains(Device.path()))
            return;
        m_batteries.remove(Device.path());
    }
    LOG(VB_GENERAL, LOG_INFO, QString("Removed UPower.Device '%1'").arg(Device.path()));
    UpdateBattery();
}

/*! \brief Update power device state.
 *
 * This is typically called by the UPower service when the battery state has
 * changed.
*/
void MythPowerDBus::DeviceChanged(const QDBusObjectPath& Device)
{
    {
        QMutexLocker locker(&s_lock);
        if (!m_batteries.contains(Device.path()))
            return;
        m_batteries[Device.path()] = RetrieveBatteryLevel(Device.path());
    }
    UpdateBattery();
}

void MythPowerDBus::UpdateProperties(void)
{
    m_features = FeatureNone;
    m_onBattery = false;

    if (m_logindInterface)
    {
        QDBusReply<QString> cansuspend = m_logindInterface->call(QLatin1String("CanSuspend"));
        if (cansuspend.isValid() && cansuspend.value() == "yes")
            m_features |= FeatureSuspend;
        QDBusReply<QString> canshutdown = m_logindInterface->call(QLatin1String("CanPowerOff"));
        if (canshutdown.isValid() && canshutdown.value() == "yes")
            m_features |= FeatureShutdown;
        QDBusReply<QString> canrestart = m_logindInterface->call(QLatin1String("CanReboot"));
        if (canrestart.isValid() && canrestart.value() == "yes")
            m_features |= FeatureRestart;
        QDBusReply<QString> canhibernate = m_logindInterface->call(QLatin1String("CanHibernate"));
        if (canhibernate.isValid() && canhibernate.value() == "yes")
            m_features |= FeatureHibernate;
        QDBusReply<QString> canhybrid = m_logindInterface->call(QLatin1String("CanHybridSleep"));
        if (canhybrid.isValid() && canhybrid.value() == "yes")
            m_features |= FeatureHybridSleep;

        QVariant delay = m_logindInterface->property("InhibitDelayMaxUSec");
        if (delay.isValid())
        {
            auto value = std::chrono::microseconds(delay.toUInt());
            m_maxSupportedDelay = duration_cast<std::chrono::seconds>(value);
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Max inhibit delay: %1seconds")
                .arg(m_maxSupportedDelay.count()));
        }
    }
}

void MythPowerDBus::UpdateBattery(void)
{
    int newlevel = UnknownPower;

    if (m_onBattery)
    {
        QMutexLocker locker(&s_lock);

        qreal total = 0;
        int   count = 0;

        // take an average (who has more than 1 battery?)
        for (int level : std::as_const(m_batteries))
        {
            if (level >= 0 && level <= 100)
            {
                count++;
                total += static_cast<qreal>(level);
            }
        }

        if (count > 0)
            newlevel = lround(total / count);
    }

    if (!m_onBattery && m_logindInterface)
    {
        QVariant acpower = m_logindInterface->property("OnExternalPower");
        if (acpower.isValid() && acpower.toBool())
            newlevel = ACPower;
    }

    PowerLevelChanged(newlevel);
}

int MythPowerDBus::RetrieveBatteryLevel(const QString &Path)
{
    QDBusInterface interface(UPOWER_SERVICE, Path, UPOWER_SERVICE + ".Device", m_bus);

    if (interface.isValid())
    {
        QVariant battery = interface.property("IsRechargeable");
        if (battery.isValid() && battery.toBool())
        {
            QVariant percent = interface.property("Percentage");
            if (percent.isValid())
            {
                int result = static_cast<int>(lroundf(percent.toFloat() * 100.0F));
                if (result >= 0 && result <= 100)
                {
                    m_onBattery = true;
                    return result;
                }
            }
        }
        else
        {
            QVariant type = interface.property("Type");
            if (type.isValid())
            {
                QString typestr = type.toString();
                if (typestr == "Line Power")
                    return ACPower;
                if (typestr == "Ups")
                    return UPS;
            }
        }
    }
    return UnknownPower;
}

/*! \brief Acquire an inhibition lock for logind power events.
 *
 * We typically acquire a lock for both suspend and shutdown. We must
 * hold this at all times (when we want to inhibit those events). Failure to hold
 * a lock may lead to race conditions and other unpredictable behaviour. When
 * we are ready, the lock is released (closed) and logind will proceed. On resume,
 * we take a new lock immediately.
 *
 * \note We only ask for delay inhibition.
 * \sa ReleaseLock
 */
void MythPowerDBus::AcquireLock(Features Types)
{
    QMutexLocker locker(&s_lock);

    if (m_lockHandle > -1)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Already hold delay lock");
        ReleaseLock();
    }

    QStringList types;
    if (Types.testFlag(FeatureSuspend)) types << "sleep";
    if (Types.testFlag(FeatureShutdown)) types << "shutdown";
    if (types.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown delay requests");
        return;
    }

    QDBusReply<QDBusUnixFileDescriptor> reply =
        m_logindInterface->call(QLatin1String("Inhibit"), types.join(":").toLocal8Bit().constData(),
                                QLatin1String("MythTV"), QLatin1String(""), QLatin1String("delay"));
    if (!reply.isValid())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Failed to delay %1: %2")
            .arg(types.join(","), reply.error().message()));
        m_lockHandle = -1;
        return;
    }

    m_lockHandle = dup(reply.value().fileDescriptor());
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Acquired delay FD: %1").arg(m_lockHandle));
}

/*! \brief Release our inhibition lock
 *
 * This will be called when we exit (cleanup) or when we are ready implement or
 * allow shutdown/resume events.
*/
void MythPowerDBus::ReleaseLock(void)
{
    QMutexLocker locker(&s_lock);
    if (m_lockHandle < 0)
        return;

    if (m_scheduledFeature)
        FeatureHappening();

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Releasing delay FD: %1").arg(m_lockHandle));
    close(m_lockHandle);
    m_lockHandle = -1;
}
