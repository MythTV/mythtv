#ifndef HARDWAREPROFILE_H_
#define HARDWAREPROFILE_H_

// QT headers
#include <QObject>
#include <QString>
#include <QDateTime>

// MythDB headers
#include "mythexp.h"

// MythUI headers
#include "mythscreentype.h"
#include "mythscreenstack.h"

class MythUIBusyDialog;

extern const QString SMOLT_SERVER_LOCATION;
extern const QString SMOLT_TOKEN;

class MPUBLIC HardwareProfile : public QObject
{
    Q_OBJECT

  public:
    HardwareProfile();
   ~HardwareProfile(void);

    void GenerateUUIDs(void);

    QString GetPrivateUUIDFromFile(void);
    bool WritePrivateUUIDToFile(QString uuid);
    QString GetPublicUUIDFromFile(void);
    QString GetAdminPasswordFromFile(void);

    bool NeedsUpdate(void);
    bool SubmitProfile(void);
    bool DeleteProfile(void);

    QString   GetPublicUUID(void) { return m_publicuuid; };
    QString   GetPrivateUUID(void) { return m_uuid; };
    QDateTime GetLastUpdate(void) { return m_lastUpdate; };
    QString   GetProfileURL(void);

  private:
    QString   m_uuid;
    QString   m_publicuuid;
    QDateTime m_lastUpdate;
    QString   m_hardwareProfile;
};

#endif
