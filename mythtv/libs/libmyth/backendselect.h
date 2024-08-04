#ifndef BACKENDSELECT_H
#define BACKENDSELECT_H

#include <QMutex>
#include <QString>

// MythTV
#include "libmythui/mythscreentype.h"
#include "libmythupnp/upnpdevice.h"

class QEventLoop;
class MythUIButtonList;
class MythUIButton;

class DatabaseParams;

// TODO: The following do not belong here, but I cannot think of a better
//       location at this moment in time
// Some common UPnP search and XML value strings
const QString kBackendURI = "urn:schemas-mythtv-org:device:MasterMediaServer:1";
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
    enum Decision : std::int8_t
    {
        kManualConfigure = -1,
        kCancelConfigure = 0,
        kAcceptConfigure = +1,
    };
    static Decision Prompt(DatabaseParams *dbParams, const QString& config_filename);

    BackendSelection(MythScreenStack *parent, DatabaseParams *params,
                     QString config_filename, bool exitOnFinish = false);
    ~BackendSelection() override;

    bool Create(void) override; // MythScreenType
    void Close(void) override; // MythScreenType
    void customEvent(QEvent *event) override; // QObject

  protected slots:
    void Accept(void);
    void Accept(MythUIButtonListItem *item);
    void Manual(void);   ///< Linked to 'Configure Manually' button
    void Cancel(void);  ///< Linked to 'Cancel' button

  private:
    void Load(void) override; // MythScreenType
    void Init(void) override; // MythScreenType
    bool ConnectBackend(DeviceLocation *dev);
    void AddItem(DeviceLocation *dev);
    void RemoveItem(const QString& USN);
    bool TryDBfromURL(const QString &error, const QString& URL);
    void PromptForPassword(void);
    void CloseWithDecision(Decision d);

    DatabaseParams   *m_dbParams        {nullptr};
    QString           m_configFilename;
    bool              m_exitOnFinish;
    ItemMap           m_devices;

    MythUIButtonList *m_backendList     {nullptr};
    MythUIButton     *m_manualButton    {nullptr};
    MythUIButton     *m_saveButton      {nullptr};
    MythUIButton     *m_cancelButton    {nullptr};

    QString           m_pinCode;
    QString           m_usn;

    QMutex            m_mutex;

    Decision          m_backendDecision {kCancelConfigure};
    QEventLoop       *m_loop            {nullptr};
};

Q_DECLARE_METATYPE(DeviceLocation*)

#endif // BACKENDSELECT_H
