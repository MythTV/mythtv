
#include "backendselect.h"

// libmyth
#include "mythcontext.h"

// libmythupnp
#include "mythxmlclient.h"
#include "configuration.h"

// libmythui
#include "mythmainwindow.h"
#include "mythdialogbox.h"
#include "mythuibutton.h"
#include "mythuistatetype.h"

// qt
#include <QApplication>
#include <QHash>

BackendSelection::BackendSelection(MythScreenStack *parent, DatabaseParams *params,
                                   XmlConfiguration *xmlconf, bool exitOnFinish)
    : MythScreenType(parent, "BackEnd Selection"),
      m_DBparams(params), m_XML(xmlconf), m_exitOnFinish(exitOnFinish),
      m_backendList(NULL), m_manualButton(NULL), m_saveButton(NULL),
      m_cancelButton(NULL)
{
}

BackendSelection::~BackendSelection()
{
    UPnp::RemoveListener(this);

    ItemMap::iterator it;
    for (it = m_devices.begin(); it != m_devices.end(); ++it)
    {
        DeviceLocation *dev = *it;

        if (dev)
            dev->Release();
    }

    m_devices.clear();
}

bool BackendSelection::m_backendChanged = false;

bool BackendSelection::prompt(DatabaseParams *dbParams,
                              XmlConfiguration *xmlConfig)
{
    m_backendChanged = false;

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    if (!mainStack)
        return false;

    BackendSelection *backendSettings = new BackendSelection(mainStack,
                                                             dbParams,
                                                             xmlConfig, true);

    if (backendSettings->Create())
    {
        mainStack->AddScreen(backendSettings, false);
        qApp->exec();
        mainStack->PopScreen(backendSettings, false);
    }
    else
        delete backendSettings;

    return m_backendChanged;
}

bool BackendSelection::Create(void)
{
    if (!LoadWindowFromXML("config-ui.xml", "backendselection", this))
        return false;

    m_backendList = dynamic_cast<MythUIButtonList*>(GetChild("backends"));
    m_saveButton = dynamic_cast<MythUIButton*>(GetChild("save"));
    m_cancelButton = dynamic_cast<MythUIButton*>(GetChild("cancel"));
    m_manualButton = dynamic_cast<MythUIButton*>(GetChild("manual"));
    //m_searchButton = dynamic_cast<MythUIButton*>(GetChild("search"));

    connect(m_backendList, SIGNAL(itemClicked(MythUIButtonListItem *)),
            SLOT(Accept(MythUIButtonListItem *)));

    // connect(m_searchButton, SIGNAL(clicked()), SLOT(Search()));
    connect(m_manualButton, SIGNAL(Clicked()), SLOT(Manual()));
    connect(m_cancelButton, SIGNAL(Clicked()), SLOT(Cancel()));
    connect(m_saveButton, SIGNAL(Clicked()), SLOT(Accept()));

    BuildFocusList();
    LoadInBackground();

    return true;
}

void BackendSelection::Accept(MythUIButtonListItem *item)
{
    if (!item)
        return;

    DeviceLocation *dev = qVariantValue<DeviceLocation *>(item->GetData());

    if (!dev)
        Close();

    if (ConnectBackend(dev))  // this does a Release()
    {
        if (m_pinCode.length())
            m_XML->SetValue(kDefaultPIN, m_pinCode);
        m_XML->SetValue(kDefaultUSN, m_USN);
        m_XML->Save();
        Close();
    }
}

void BackendSelection::Accept(void)
{
    MythUIButtonListItem *item = m_backendList->GetItemCurrent();

    if (!item)
        return;

    Accept(item);
}


void BackendSelection::AddItem(DeviceLocation *dev)
{
    if (!dev)
        return;

    QString USN = dev->m_sUSN;

    // The devices' USN should be unique. Don't add if it is already there:
    if (m_devices.find(USN) == m_devices.end())
    {

        InfoMap infomap;
        dev->GetDeviceDetail(infomap, true);

        // We only want the version number, not the library version info
        infomap["version"] = infomap["modelnumber"].section('.', 0, 1);

        MythUIButtonListItem *item;
        item = new MythUIButtonListItem(m_backendList, infomap["modelname"],
                                        qVariantFromValue(dev));
        item->SetTextFromMap(infomap);

        bool protoMatch = (infomap["protocolversion"] == MYTH_PROTO_VERSION);

        QString status = "good";
        if (!protoMatch)
            status = "protocolmismatch";

        // TODO: Not foolproof but if we can't get device details then it's
        // probably because we could not connect to port 6544 - firewall?
        // Maybe we can replace this with a more specific check
        if (infomap["modelname"].isEmpty())
            status = "blocked";

        item->DisplayState(status, "connection");

        bool needPin = dev->NeedSecurityPin();
        item->DisplayState(needPin ? "yes" : "no", "securitypin");

        dev->AddRef();
        m_devices.insert(USN, dev);
    }

    dev->Release();
}

/**
 * Attempt UPnP connection to a backend device, get its DB details.
 * Will loop until a valid PIN is entered.
 */
bool BackendSelection::ConnectBackend(DeviceLocation *dev)
{
    QString          error;
    QString          message;
    UPnPResultCode   stat;
    MythXMLClient   *xml;

    m_USN = dev->m_sUSN;
    xml   = new MythXMLClient(dev->m_sLocation);
    stat  = xml->GetConnectionInfo(m_pinCode, m_DBparams, message);
    QString backendName = dev->GetFriendlyName(true);
    if (backendName == "<Unknown>")
        backendName = dev->m_sLocation;

    switch (stat)
    {
        case UPnPResult_Success:
            VERBOSE(VB_UPNP, QString("ConnectBackend() - success. New hostname: %1")
                    .arg(m_DBparams->dbHostName));
            return true;

        case UPnPResult_HumanInterventionRequired:
            VERBOSE(VB_UPNP, error);
            ShowOkPopup(message);
            if (TryDBfromURL("", dev->m_sLocation))
            {
                delete xml;
                return true;
            }
            break;

        case UPnPResult_ActionNotAuthorized:
            VERBOSE(VB_UPNP, QString("Access denied for %1. Wrong PIN?")
                    .arg(backendName));
            PromptForPassword();
            break;
        default:
            VERBOSE(VB_UPNP, QString("GetConnectionInfo() failed for %1")
                    .arg(backendName));
            ShowOkPopup(message);
    }

    // Back to the list, so the user can choose a different backend:
    SetFocusWidget(m_backendList);
    delete xml;
    return false;
}

void BackendSelection::Cancel()
{
    Close();
}

void BackendSelection::Load()
{
    UPnp::AddListener(this);
    UPnp::PerformSearch(gBackendURI);
}

void BackendSelection::Init(void)
{
    EntryMap::Iterator  it;
    EntryMap            ourMap;
    DeviceLocation     *pDevLoc;

    SSDPCacheEntries *pEntries = UPnp::g_SSDPCache.Find(gBackendURI);

    if (!pEntries)
    {
        VERBOSE(VB_GENERAL, "Found zero backends, bailing");
        return;
    }

    pEntries->AddRef();
    pEntries->Lock();

    EntryMap *pMap = pEntries->GetEntryMap();

    for (it = pMap->begin(); it != pMap->end(); ++it)
    {
        pDevLoc = (DeviceLocation *)*it;

        if (!pDevLoc)
            continue;

        pDevLoc->AddRef();
        ourMap.insert(pDevLoc->m_sUSN, pDevLoc);
    }

    pEntries->Unlock();
    pEntries->Release();

    for (it = ourMap.begin(); it != ourMap.end(); ++it)
    {
        pDevLoc = (DeviceLocation *)*it;
        AddItem(pDevLoc);   // this does a Release()
    }
}

void BackendSelection::Manual(void)
{
    Close();
}

void BackendSelection::RemoveItem(QString USN)
{
    ItemMap::iterator it = m_devices.find(USN);

    if (it != m_devices.end())
    {
        DeviceLocation *dev = *it;

        if (dev)
            dev->Release();

        m_devices.erase(it);
    }
}

// void BackendSelection::Search(void)
// {
//     UPnp::PerformSearch(gBackendURI);
// }

bool BackendSelection::TryDBfromURL(const QString &error, QString URL)
{
    if (ShowOkPopup(error + tr("Shall I attempt to connect to this"
                    " host with default database parameters?")))
    {
        URL.remove("http://");
        URL.remove(QRegExp("[:/].*"));
        m_DBparams->dbHostName = URL;
        return true;
    }

    return false;
}

void BackendSelection::customEvent(QEvent *event)
{
    if (((MythEvent::Type)(event->type())) == MythEvent::MythEventMessage)
    {
        MythEvent *me      = (MythEvent *)event;
        QString    message = me->Message();
        QString    URI     = me->ExtraData(0);
        QString    URN     = me->ExtraData(1);
        QString    URL     = me->ExtraData(2);


        VERBOSE(VB_UPNP, QString("BackendSelection::customEvent(%1, %2, %3, %4)")
                .arg(message).arg(URI).arg(URN).arg(URL));

        if (message.startsWith("SSDP_ADD") &&
            URI.startsWith("urn:schemas-mythtv-org:device:MasterMediaServer:"))
        {
            DeviceLocation *devLoc = UPnp::g_SSDPCache.Find(URI, URN);

            if (devLoc)
            {
                devLoc->AddRef();
                AddItem(devLoc);   // this does a Release()
            }
        }
        else if (message.startsWith("SSDP_REMOVE"))
        {
            //-=>Note: This code will never get executed until
            //         SSDPCache is changed to handle NotifyRemove correctly
            RemoveItem(URN);
        }
    }
    else if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = dynamic_cast<DialogCompletionEvent*>(event);

        if (!dce)
            return;

        QString resultid = dce->GetId();

        if (resultid == "password")
        {
            m_pinCode = dce->GetResultText();
            Accept();
        }
    }
}

void BackendSelection::PromptForPassword(void)
{
    QString message = tr("Please enter the backend access PIN");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythTextInputDialog *pwDialog = new MythTextInputDialog(popupStack,
                                                            message,
                                                            FilterNone,
                                                            true);

    if (pwDialog->Create())
    {
        pwDialog->SetReturnEvent(this, "password");
        popupStack->AddScreen(pwDialog);
    }
    else
        delete pwDialog;
}

void BackendSelection::Close(void)
{
    if (m_exitOnFinish)
        qApp->quit();
    else
        MythScreenType::Close();
}
