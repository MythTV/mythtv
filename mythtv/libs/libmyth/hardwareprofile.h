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

class MPUBLIC HardwareProfile : public QObject
{
    Q_OBJECT

  public:
    HardwareProfile();
   ~HardwareProfile(void);

    bool Prompt(bool force = false);
    void ShowPrompt(void);

    void GenerateUUID(void);
    bool SubmitResults(void);

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
