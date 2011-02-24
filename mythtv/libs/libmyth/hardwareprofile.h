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

    bool Prompt(bool force = false);
    void ShowPrompt(void);

    void GenerateUUID(void);
    QString GetUUIDFromFile(void);
    bool WriteUUIDToFile(QString uuid);

    bool SubmitProfile(void);
    bool DeleteProfile(void);

  public slots:
    void OnPromptReturn(bool submit);

  private:
    void CreateBusyDialog(QString message);

    MythScreenStack *m_popupStack;
    MythUIBusyDialog *m_busyPopup;

    QString m_uuid;
    QString m_hardwareProfile;
};

#endif
