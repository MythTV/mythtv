#ifndef HARDWAREPROFILE_H_
#define HARDWAREPROFILE_H_

// QT headers
#include <QObject>
#include <QString>
#include <QDateTime>

// MythDB headers
#include "mythbaseexp.h"
#include "housekeeper.h"

extern const QString SMOLT_SERVER_LOCATION;
extern const QString SMOLT_TOKEN;

class MBASE_PUBLIC HardwareProfile : public QObject
{
    Q_OBJECT

  public:
    HardwareProfile();
   ~HardwareProfile(void);

    void Enable(void);
    void Disable(void);

    void GenerateUUIDs(void);

    QString GetPrivateUUIDFromFile(void) const;
    bool WritePrivateUUIDToFile(QString uuid);
    QString GetPublicUUIDFromFile(void) const;
    QString GetAdminPasswordFromFile(void) const;

    bool NeedsUpdate(void) const;
    bool SubmitProfile(bool updateTime=true);
    bool DeleteProfile(void);

    QString   GetPublicUUID(void) const { return m_publicuuid; };
    QString   GetPrivateUUID(void) const { return m_uuid; };
    QDateTime GetLastUpdate(void) const { return m_lastUpdate; };
    QString   GetProfileURL(void) const;
    QString   GetHardwareProfile(void) const;

  private:
    bool      m_enabled;
    QString   m_uuid;
    QString   m_publicuuid;
    QDateTime m_lastUpdate;
    QString   m_hardwareProfile;
};

class MBASE_PUBLIC HardwareProfileTask : public PeriodicHouseKeeperTask
{
  public:
    HardwareProfileTask(void) : PeriodicHouseKeeperTask("HardwareProfiler",
                                            2592000,  // 30 days in seconds
                                            0.96667f, // up to one day early
                                            1.03333f, // up to one day late
                                            kHKLocal, kHKRunOnStartup) {}
    virtual bool DoCheckRun(QDateTime now);
    virtual bool DoRun(void);
  private:

};

#endif
