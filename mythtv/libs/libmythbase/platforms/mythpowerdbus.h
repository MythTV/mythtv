#ifndef MYTHPOWERDBUS_H
#define MYTHPOWERDBUS_H

// Qt
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>

// MythTV
#include "mythpower.h"

class MythPowerDBus : public MythPower
{
    Q_OBJECT

    friend class MythPower;

  public:
    static bool IsAvailable(void);

  public slots:
    void CancelFeature(void) override;
    void Changed          (void);
    void DeviceAdded      (const QDBusObjectPath& Device);
    void DeviceRemoved    (const QDBusObjectPath& Device);
    void DeviceChanged    (const QDBusObjectPath& Device);
    void DBusSuspending   (bool Stopping);
    void DBusShuttingDown (bool Stopping);

  protected slots:
    void ReleaseLock      (void);

  protected:
    MythPowerDBus();
   ~MythPowerDBus() override;

    void Init             (void) override;
    bool DoFeature        (bool Delayed = false) override;
    void DidWakeUp        (void) override;
    bool ScheduleFeature  (enum Feature Type, std::chrono::seconds Delay) override;

  private:
    bool UpdateStatus        (void);
    int  RetrieveBatteryLevel(const QString &Path);
    void UpdateBattery       (void);
    void UpdateProperties    (void);
    void AcquireLock         (Features Types);

    bool               m_onBattery        { false };
    QMap<QString,int>  m_batteries;
    QDBusConnection    m_bus              { QDBusConnection::systemBus() };
    QDBusInterface    *m_upowerInterface  { nullptr };
    QDBusInterface    *m_logindInterface  { nullptr };
    int                m_lockHandle       { -1 };
    QTimer             m_delayTimer;
};

#endif // MYTHPOWERDBUS_H
