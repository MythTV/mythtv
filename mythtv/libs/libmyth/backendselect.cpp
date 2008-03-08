/**
 * \class BackendSelect
 * \desc  Classes to Prompt user for a master backend.
 *
 * \author Originally based on masterselection.cpp/h by David Blain.
 */

#include "backendselect.h"
#include "mythdialogs.h"
#include "mythconfigdialogs.h"

#include "libmythupnp/mythxmlclient.h"


BackendSelect::BackendSelect(MythMainWindow *parent, DatabaseParams *params)
             : MythDialog(parent, "BackEnd Selection", TRUE)
{
    m_parent   = parent;
    m_DBparams = params;

    CreateUI();

    UPnp::PerformSearch(gBackendURI);
    UPnp::AddListener(this);

    FillListBox();

    m_backends->setFocus();
}

BackendSelect::~BackendSelect()
{
    UPnp::RemoveListener(this);

    ItemMap::iterator it;
    for (it = m_devices.begin(); it != m_devices.end(); ++it)
    {
        ListBoxDevice *item = it.data();

        if (item != NULL)
            delete item;
    }

    m_devices.clear();
}

void BackendSelect::Accept(void)
{
    DeviceLocation *dev;
    QListBoxItem   *selected = m_backends->selectedItem();

    if (!selected)
        return;

    dev = ((ListBoxDevice *)selected)->m_dev;

    if (!dev)
        reject();

    dev->AddRef();
    if (Connect(dev))  // this does a Release()
        accept();
}

void BackendSelect::AddItem(DeviceLocation *dev)
{
    if (!dev)
        return;

    QString USN = dev->m_sUSN;

    // The devices' USN should be unique. Don't add if it is already there:
    if (m_devices.find(USN) == m_devices.end())
    {
        ListBoxDevice *item;
        QString        name;

        if (print_verbose_messages & VB_UPNP)
            name = dev->GetNameAndDetails(true);
        else
            name = dev->GetFriendlyName(true);

        item = new ListBoxDevice(m_backends, name, dev);
        m_devices.insert(USN, item);

        // Pre-select at least one item:
        if (m_backends->numRows() == 1)
            m_backends->setSelected(0, true);
    }

    dev->Release();
}

/**
 * Attempt UPnP connection to a backend device, get its DB details.
 * Will loop until a valid PIN is entered.
 */
bool BackendSelect::Connect(DeviceLocation *dev)
{
    QString          error;
    QString          message;
    UPnPResultCode   stat;
    MythXMLClient   *xml;

    m_USN = dev->m_sUSN;
    xml   = new MythXMLClient(dev->m_sLocation);
    stat  = xml->GetConnectionInfo(m_PIN, m_DBparams, message);
    error = dev->GetFriendlyName(true);
    if (error == "<Unknown>")
        error = dev->m_sLocation;
    error += ". " + message;
    dev->Release();

    switch (stat)
    {
        case UPnPResult_Success:
            VERBOSE(VB_UPNP, "Connect() - success. New hostname: "
                             + m_DBparams->dbHostName);
            return true;

        case UPnPResult_HumanInterventionRequired:
            VERBOSE(VB_UPNP, error);
            MythPopupBox::showOkPopup(m_parent, "", tr(message));
            break;

        case UPnPResult_ActionNotAuthorized:
            VERBOSE(VB_UPNP, "Access denied for " + error + ". Wrong PIN?");
            message = "Please enter the backend access PIN";
            do
            {
                m_PIN = MythPopupBox::showPasswordPopup(
                    m_parent, "Backend PIN entry", tr(message));
                stat = xml->GetConnectionInfo(m_PIN, m_DBparams, message);
            }
            while (stat == UPnPResult_ActionNotAuthorized);
            if (stat == UPnPResult_Success)
                return true;

        default:
            VERBOSE(VB_UPNP, "GetConnectionInfo() failed for " + error);
            MythPopupBox::showOkPopup(m_parent, "", tr(message));
    }

    // Back to the list, so the user can choose a different backend:
    m_backends->setFocus();
    return false;
}

void BackendSelect::CreateUI(void)
{
    // Probably should all be members, and deleted by ~BackendSelect()
    QLabel         *label;
    QGridLayout    *layout;
    MythPushButton *cancel;
    MythPushButton *manual;
    MythPushButton *OK;
    //MythPushButton *search;   // I don't see the need for this?


    label = new QLabel(tr("Please select default Myth Backend Server"), this);
    label->setBackgroundOrigin(QWidget::WindowOrigin);

    m_backends = new MythListBox(this);
    OK         = new MythPushButton(tr("OK"), this);
    cancel     = new MythPushButton(tr("Cancel"), this);
    manual     = new MythPushButton(tr("Configure Manually"), this);
    //search     = new MythPushButton(tr("Search"), this);


    layout = new QGridLayout(this, 5, 5, 40);
    layout->addMultiCellWidget(label, 0, 0, 1, 3);
    layout->addMultiCellWidget(m_backends, 1, 1, 0, 4);

    layout->addMultiCellWidget(manual, 4, 4, 0, 1);
    //layout->addWidget(search, 4, 0);
    //layout->addWidget(manual, 4, 1);
    layout->addWidget(cancel, 4, 3);
    layout->addWidget(OK    , 4, 4);


    connect(m_backends, SIGNAL(accepted(int)), SLOT(Accept()));
    //connect(search,     SIGNAL(clicked()),     SLOT(Search()));
    connect(manual,     SIGNAL(clicked()),     SLOT(Manual()));
    connect(cancel,     SIGNAL(clicked()),     SLOT(reject()));
    connect(OK,         SIGNAL(clicked()),     SLOT(Accept()));
}

void BackendSelect::customEvent(QCustomEvent *e)
{
    if (MythEvent::MythEventMessage != ((MythEvent::Type)(e->type())))
        return;


    MythEvent *me      = (MythEvent *)e;
    QString    message = me->Message();
    QString    URI     = me->ExtraData(0);
    QString    URN     = me->ExtraData(1);
    QString    URL     = me->ExtraData(2);


    VERBOSE(VB_UPNP, "BackendSelect::customEvent(" + message
                     + ", " + URI + ", " + URN + ", " + URL + ")");

    if (message.startsWith("SSDP_ADD") &&
        URI.startsWith("urn:schemas-mythtv-org:device:MasterMediaServer:"))
    {
        DeviceLocation *devLoc = UPnp::g_SSDPCache.Find(URI, URN);

        if (devLoc != NULL)
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

void BackendSelect::FillListBox(void)
{
    EntryMap::Iterator  it;
    EntryMap            ourMap;
    DeviceLocation     *pDevLoc;
    

    SSDPCacheEntries *pEntries = UPnp::g_SSDPCache.Find(gBackendURI);

    if (!pEntries)
        return;

    pEntries->AddRef();
    pEntries->Lock();

    EntryMap *pMap = pEntries->GetEntryMap();

    for (it = pMap->begin(); it != pMap->end(); ++it)
    {
        pDevLoc = (DeviceLocation *)it.data();

        if (!pDevLoc)
            continue;

        pDevLoc->AddRef();
        ourMap.insert(pDevLoc->m_sUSN, pDevLoc);
    }


    pEntries->Unlock();
    pEntries->Release();

    for (it = ourMap.begin(); it != ourMap.end(); ++it)
    {
        pDevLoc = (DeviceLocation *)it.data();
        AddItem(pDevLoc);   // this does a Release()
    }
}

void BackendSelect::Manual(void)
{
    done(kDialogCodeButton0);
}

void BackendSelect::RemoveItem(QString USN)
{
    ItemMap::iterator it = m_devices.find(USN);

    if (it != m_devices.end())
    {
        ListBoxDevice *item = it.data();

        if (item != NULL)
            delete item;

        m_devices.remove(it);
    }
}

//void BackendSelect::Search(void)
//{
//    UPnp::PerformSearch(gBackendURI);
//}
