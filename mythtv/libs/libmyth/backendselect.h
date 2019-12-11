#ifndef __BACKENDSELECT_H__
#define __BACKENDSELECT_H__

#include <QMutex>

// libmythui
#include "mythscreentype.h"

#include "configuration.h"
#include "upnpdevice.h"

class QEventLoop;
class MythUIButtonList;
class MythUIButton;

class DatabaseParams;

// TODO: The following do not belong here, but I cannot think of a better
//       location at this moment in time
// Some common UPnP search and XML value strings
const QString gBackendURI = "urn:schemas-mythtv-org:device:MasterMediaServer:1";
const QString kDefaultDB  = "Database/";
const QString kDefaultWOL = "WakeOnLAN/";
const QString kDefaultMFE = "UPnP/MythFrontend/DefaultBackend/";
const QString kDefaultPIN = kDefaultMFE + "SecurityPin";
const QString kDefaultUSN = kDefaultMFE + "USN";

using ItemMap = QMap <QString, DeviceLocation*>;

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
    enum Decision
    {
        kManualConfigure = -1,
        kCancelConfigure = 0,
        kAcceptConfigure = +1,
    };
    static Decision Prompt(
        DatabaseParams *dbParams, Configuration *pConfig);

    BackendSelection(MythScreenStack *parent, DatabaseParams *params,
                     Configuration *pConfig, bool exitOnFinish = false);
    virtual ~BackendSelection();

    bool Create(void) override; // MythScreenType
    void Close(void) override; // MythScreenType
    void customEvent(QEvent *event) override; // QObject

  protected slots:
    void Accept(void);
    void Accept(MythUIButtonListItem *);
    void Manual(void);   ///< Linked to 'Configure Manually' button
    void Cancel(void);  ///< Linked to 'Cancel' button

  private:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType
    bool ConnectBackend(DeviceLocation *dev);
    void AddItem(DeviceLocation *dev);
    void RemoveItem(const QString& USN);
    bool TryDBfromURL(const QString &error, QString URL);
    void PromptForPassword(void);
    void CloseWithDecision(Decision);

    DatabaseParams   *m_dbParams        {nullptr};
    Configuration    *m_pConfig         {nullptr};
    bool              m_exitOnFinish;
    ItemMap           m_devices;

    MythUIButtonList *m_backendList     {nullptr};
    MythUIButton     *m_manualButton    {nullptr};
    MythUIButton     *m_saveButton      {nullptr};
    MythUIButton     *m_cancelButton    {nullptr};
    //MythUIButton   *m_searchButton    {nullptr};

    QString           m_pinCode;
    QString           m_usn;

    QMutex            m_mutex;

    Decision          m_backendDecision {kCancelConfigure};
    QEventLoop       *m_loop            {nullptr};
};

Q_DECLARE_METATYPE(DeviceLocation*)

#endif
