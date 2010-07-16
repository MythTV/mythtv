/**
 * \class BackendSelect
 * \brief Classes to Prompt user for a master backend.
 *
 * \author Originally based on masterselection.cpp/h by David Blain.
 */

#include "backendselect.h"
#include "mythdialogs.h"
#include "mythconfigdialogs.h"
#include "mythcontext.h"

#include "mythxmlclient.h"


BackendSelect::BackendSelect(MythMainWindow *parent, DatabaseParams *params)
    : MythDialog(parent, "BackEnd Selection", true),
      m_PIN(QString::null), m_USN(QString::null),
      m_DBparams(params), m_parent(parent), m_backends(NULL)
{
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
        ListBoxDevice *item = *it;

        if (item != NULL)
            delete item;
    }

    m_devices.clear();
}

void BackendSelect::Accept(QListWidgetItem *item)
{
    DeviceLocation *dev;
    ListBoxDevice  *selected = dynamic_cast<ListBoxDevice*>(item);

    if (!selected)
        return;

    dev = selected->m_dev;

    if (!dev)
        reject();

    dev->AddRef();
    if (Connect(dev))  // this does a Release()
        accept();
}

void BackendSelect::Accept(void)
{
    QList<QListWidgetItem *>  selections = m_backends->selectedItems();

    if (selections.empty())
    {
        VERBOSE(VB_IMPORTANT,
                "BackendSelect::Accept() - no QListWidget selected?");
        return;
    }

    Accept(selections[0]);
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

        if (VERBOSE_LEVEL_CHECK(VB_UPNP))
            name = dev->GetNameAndDetails(true);
        else
            name = dev->GetFriendlyName(true);

        item = new ListBoxDevice(m_backends, name, dev);
        m_devices.insert(USN, item);

        // Pre-select at least one item:
        if (m_backends->count() == 1)
            m_backends->setCurrentRow(0);
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
            MythPopupBox::showOkPopup(m_parent, "",
                                      tr(message.toLatin1().constData()));
            if (TryDBfromURL("", dev->m_sLocation))
            {
                delete xml;
                return true;
            }
            break;

        case UPnPResult_ActionNotAuthorized:
            VERBOSE(VB_UPNP, "Access denied for " + error + ". Wrong PIN?");
            if (TryDBfromURL(tr("Backend uses a PIN. "), dev->m_sLocation))
                return true;
            message = "Please enter the backend access PIN";
            do
            {
                m_PIN = MythPopupBox::showPasswordPopup(
                    m_parent, "Backend PIN entry",
                    tr(message.toLatin1().constData()));

                // User might have cancelled?
                if (m_PIN.isEmpty())
                    break;

                stat = xml->GetConnectionInfo(m_PIN, m_DBparams, message);
            }
            while (stat == UPnPResult_ActionNotAuthorized);
            if (stat == UPnPResult_Success)
            {
                delete xml;
                return true;
            }
            break;

        default:
            VERBOSE(VB_UPNP, "GetConnectionInfo() failed for " + error);
            MythPopupBox::showOkPopup(m_parent, "",
                                      tr(message.toLatin1().constData()));
    }

    // Back to the list, so the user can choose a different backend:
    m_backends->setFocus();
    delete xml;
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
#ifdef SEARCH_BUTTON
    MythPushButton *search;
#endif


    label = new QLabel(tr("Please select default MythTV Backend Server"), this);

    m_backends = new QListWidget(this);
    OK         = new MythPushButton(tr("OK"), this);
    cancel     = new MythPushButton(tr("Cancel"), this);
    manual     = new MythPushButton(tr("Configure Manually"), this);
#ifdef SEARCH_BUTTON
    search     = new MythPushButton(tr("Search"), this);
#endif


    layout = new QGridLayout(this);
    layout->setContentsMargins(40,40,40,40);
    layout->addWidget(label, 0, 1, 1, 3);
    layout->addWidget(m_backends, 1, 0, 1, 5);

#ifdef SEARCH_BUTTON
    layout->addWidget(search, 4, 0);
    layout->addWidget(manual, 4, 1, 1, 2);
#else
    layout->addWidget(manual, 4, 0, 1, 2);
#endif
    layout->addWidget(cancel, 4, 3);
    layout->addWidget(OK    , 4, 4);


    // Catch Escape/Enter/Return key
    m_backends->installEventFilter(this);

    // Mouse double click on a list item
    connect(m_backends, SIGNAL(itemActivated(QListWidgetItem *)),
                                 SLOT(Accept(QListWidgetItem *)));
#ifdef SEARCH_BUTTON
    connect(search,     SIGNAL(clicked()),     SLOT(Search()));
#endif
    connect(manual,     SIGNAL(clicked()),     SLOT(Manual()));
    connect(cancel,     SIGNAL(clicked()),     SLOT(reject()));
    connect(OK,         SIGNAL(clicked()),     SLOT(Accept()));
}

void BackendSelect::customEvent(QEvent *e)
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

/**
 * Allow key shortcuts in the backend list widget.
 *
 * Note that this has to use Qt style event codes, instead of the QStrings
 * returned by TranslateKeyPress(), because there is no translation table yet.
 */
bool BackendSelect::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        int key = ((QKeyEvent*)event)->key();

        if (key == Qt::Key_Return || key == Qt::Key_Enter)
        {
            Accept();
            return true;
        }
        else if (key == Qt::Key_Escape)
        {
            reject();
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
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

void BackendSelect::Manual(void)
{
    done(kDialogCodeButton0);
}

void BackendSelect::RemoveItem(QString USN)
{
    ItemMap::iterator it = m_devices.find(USN);

    if (it != m_devices.end())
    {
        ListBoxDevice *item = *it;

        if (item != NULL)
            delete item;

        m_devices.erase(it);
    }
}

#ifdef SEARCH_BUTTON
void BackendSelect::Search(void)
{
    UPnp::PerformSearch(gBackendURI);
}
#endif

bool BackendSelect::TryDBfromURL(const QString &error, QString URL)
{
    if (MythPopupBox::showOkCancelPopup(m_parent, "",
            error + tr("Shall I attempt to connect to this"
                       " host with default database parameters?"), true))
    {
        URL.remove("http://");
        URL.remove(QRegExp("[:/].*"));
        m_DBparams->dbHostName = URL;
        return true;
    }

    return false;
}

