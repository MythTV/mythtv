#ifndef HARDWAREPROFILE_H_
#define HARDWAREPROFILE_H_

// QT headers
#include <QObject>
#include <QString>
#include <QDateTime>

// MythDB headers
#include "mythbaseexp.h"

extern const QString SMOLT_SERVER_LOCATION;
extern const QString SMOLT_TOKEN;

class MBASE_PUBLIC HardwareProfile : public QObject
{
    Q_OBJECT

  public:
    HardwareProfile();
   ~HardwareProfile(void);

    void GenerateUUIDs(void);

    QString GetPrivateUUIDFromFile(void) const;
    bool WritePrivateUUIDToFile(QString uuid);
    QString GetPublicUUIDFromFile(void) const;
    QString GetAdminPasswordFromFile(void) const;

    bool NeedsUpdate(void) const;
    bool SubmitProfile(void);
    bool DeleteProfile(void);

    QString   GetPublicUUID(void) const { return m_publicuuid; };
    QString   GetPrivateUUID(void) const { return m_uuid; };
    QDateTime GetLastUpdate(void) const { return m_lastUpdate; };
    QString   GetProfileURL(void) const;
    QString   GetHardwareProfile(void) const;

  private:
    QString   m_uuid;
    QString   m_publicuuid;
    QDateTime m_lastUpdate;
    QString   m_hardwareProfile;
};

#endif
