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
   ~HardwareProfile(void) = default;

    void Enable(void);
    static void Disable(void);

    void GenerateUUIDs(void);

    static QString GetPrivateUUIDFromFile(void) ;
    static bool WritePrivateUUIDToFile(const QString &uuid);
    QString GetPublicUUIDFromFile(void) const;
    static QString GetAdminPasswordFromFile(void) ;

    bool NeedsUpdate(void) const;
    bool SubmitProfile(bool updateTime=true);
    bool DeleteProfile(void);

    QString   GetPublicUUID(void) const { return m_publicuuid; };
    QString   GetPrivateUUID(void) const { return m_uuid; };
    QDateTime GetLastUpdate(void) const { return m_lastUpdate; };
    QString   GetProfileURL(void) const;
    static QString   GetHardwareProfile(void) ;

  private:
    bool      m_enabled {false};
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
                                            86400, // retry daily on error
                                            kHKLocal, kHKRunOnStartup) {}
    bool DoCheckRun(QDateTime now) override; // HouseKeeperTask
    bool DoRun(void) override; // HouseKeeperTask
  private:

};

#endif
