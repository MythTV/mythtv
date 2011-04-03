#ifndef __BACKENDSELECT_H__
#define __BACKENDSELECT_H__

#include <QMutex>

// libmythui
#include "mythscreentype.h"
#include "mythuibuttonlist.h"

#include "configuration.h"
#include "upnpdevice.h"

class MythUIButtonList;
class MythUIButton;

struct DatabaseParams;

// TODO: The following do not belong here, but I cannot think of a better
//       location at this moment in time
// Some common UPnP search and XML value strings
const QString gBackendURI = "urn:schemas-mythtv-org:device:MasterMediaServer:1";
const QString kDefaultBE  = "UPnP/MythFrontend/DefaultBackend/";
const QString kDefaultPIN = kDefaultBE + "SecurityPin";
const QString kDefaultUSN = kDefaultBE + "USN";

typedef QMap <QString, DeviceLocation*> ItemMap;

/**
 * \class BackendSelection
 * \brief Classes to Prompt user for a master backend.
 *
 * \author Originally based on masterselection.cpp/h by David Blain.
 */

class BackendSelection : public MythScreenType
{
    Q_OBJECT

  public:
    BackendSelection(MythScreenStack *parent, DatabaseParams *params,
                     Configuration *pConfig, bool exitOnFinish = false);
    virtual ~BackendSelection();

    bool Create(void);
    void customEvent(QEvent *event);

    static bool prompt(DatabaseParams *dbParams, Configuration *pConfig);

  signals:
//    void

  public slots:
    void Accept(void);
    void Accept(MythUIButtonListItem *);
    void Manual(void);   ///< Linked to 'Configure Manually' button
    void Cancel(void);  ///< Linked to 'Cancel' button
    void Close(void);

  private:
    void Load(void);
    void Init(void);
    bool ConnectBackend(DeviceLocation *dev);
    void AddItem(DeviceLocation *dev);
    void RemoveItem(QString URN);
    bool TryDBfromURL(const QString &error, QString URL);
    void PromptForPassword(void);

    DatabaseParams *m_DBparams;
    Configuration  *m_pConfig;
    bool m_exitOnFinish;
    ItemMap m_devices;

    MythUIButtonList *m_backendList;
    MythUIButton *m_manualButton;
    MythUIButton *m_saveButton;
    MythUIButton *m_cancelButton;
    //MythUIButton *m_searchButton;

    QString m_pinCode;
    QString m_USN;

    QMutex  m_mutex;

    static bool m_backendChanged;
};

Q_DECLARE_METATYPE(DeviceLocation*)

#endif
