#ifndef HARDWAREPROFILE_H_
#define HARDWAREPROFILE_H_

// QT headers
#include <QObject>
#include <QString>

// MythDB headers
#include "mythexp.h"

// MythUI headers
#include "mythscreentype.h"
#include "mythscreenstack.h"

class MythUIBusyDialog;

extern const QString SMOLT_SERVER_LOCATION;

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

    bool SubmitProfile(void);
    bool DeleteProfile(void);

    QString GetPublicUUID(void) {return m_publicuuid; };
    QString GetPrivateUUID(void) {return m_uuid; };

    QString GetProfileURL(void);

  private:
    QString m_uuid;
    QString m_publicuuid;
    QString m_hardwareProfile;
};

#endif
