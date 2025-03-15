// -*- Mode: c++ -*-
#include "backendselect.h"

#include <utility>

#include <QEventLoop>

#include "libmythbase/configuration.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuistatetype.h"
#include "libmythupnp/mythxmlclient.h"
#include "libmythupnp/ssdp.h"
#include "libmythupnp/ssdpcache.h"

BackendSelection::BackendSelection(
    MythScreenStack *parent,
    DatabaseParams *params,
    QString config_filename,
    bool exitOnFinish
    ) :
    MythScreenType(parent, "BackEnd Selection"),
    m_dbParams(params),
    m_configFilename(std::move(config_filename)),
    m_exitOnFinish(exitOnFinish)
{
    if (exitOnFinish)
    {
        m_loop = new QEventLoop();
    }
}

BackendSelection::~BackendSelection()
{
    SSDP::RemoveListener(this);

    ItemMap::iterator it;
    for (it = m_devices.begin(); it != m_devices.end(); ++it)
    {
        if (*it)
            (*it)->DecrRef();
    }

    m_devices.clear();

    if (m_exitOnFinish)
    {
        delete m_loop;
    }
}

BackendSelection::Decision BackendSelection::Prompt(
    DatabaseParams *dbParams, const QString& config_filename)
{
    Decision ret = kCancelConfigure;
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    if (!mainStack)
        return ret;

    auto *backendSettings =
        new BackendSelection(mainStack, dbParams, config_filename, true);

    if (backendSettings->Create())
    {
        mainStack->AddScreen(backendSettings, false);
        backendSettings->m_loop->exec();
        ret = backendSettings->m_backendDecision;
        mainStack->PopScreen(backendSettings, false);
    }
    else
    {
        delete backendSettings;
    }

    return ret;
}

bool BackendSelection::Create(void)
{
    if (!LoadWindowFromXML("config-ui.xml", "backendselection", this))
        return false;

    m_backendList = dynamic_cast<MythUIButtonList*>(GetChild("backends"));
    m_saveButton = dynamic_cast<MythUIButton*>(GetChild("save"));
    m_cancelButton = dynamic_cast<MythUIButton*>(GetChild("cancel"));
    m_manualButton = dynamic_cast<MythUIButton*>(GetChild("manual"));

    connect(m_backendList, &MythUIButtonList::itemClicked,
            this, qOverload<MythUIButtonListItem *>(&BackendSelection::Accept));

    connect(m_manualButton, &MythUIButton::Clicked, this, &BackendSelection::Manual);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &BackendSelection::Cancel);
    connect(m_saveButton, &MythUIButton::Clicked,
            this, qOverload<>(&BackendSelection::Accept));

    BuildFocusList();
    LoadInBackground();

    return true;
}

void BackendSelection::Accept(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *dev = item->GetData().value<DeviceLocation *>();
    if (!dev)
    {
        Cancel();
        LOG(VB_GENERAL, LOG_ERR,
            "Could not get device details from UI element?");
        return;
    }

    if (ConnectBackend(dev))
    {
        {
            auto config = XmlConfiguration(m_configFilename);
            if (!m_pinCode.isEmpty())
                config.SetValue(XmlConfiguration::kDefaultPIN, m_pinCode);
            config.SetValue(XmlConfiguration::kDefaultUSN, m_usn);
            config.Save();
        }
        CloseWithDecision(kAcceptConfigure);
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

    QString usn = dev->m_sUSN;

    m_mutex.lock();

    // The devices' USN should be unique. Don't add if it is already there:
    if (!m_devices.contains(usn))
    {
        dev->IncrRef();
        m_devices.insert(usn, dev);

        m_mutex.unlock();

        InfoMap infomap;
        dev->GetDeviceDetail(infomap);

        // We only want the version number, not the library version info
        infomap["version"] = infomap["modelnumber"].section('.', 0, 1);

        auto *item = new MythUIButtonListItem(m_backendList, infomap["modelname"],
                                              QVariant::fromValue(dev));
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
    }
    else
    {
        m_mutex.unlock();
    }
}

/**
 * Attempt UPnP connection to a backend device, get its DB details.
 * Will loop until a valid PIN is entered.
 */
bool BackendSelection::ConnectBackend(DeviceLocation *dev)
{
    QString          message;

    m_usn   = dev->m_sUSN;

    MythXMLClient client( dev->m_sLocation );

    UPnPResultCode stat = client.GetConnectionInfo(m_pinCode, m_dbParams, message);

    QString backendName = dev->GetFriendlyName();

    if (backendName == "<Unknown>")
        backendName = dev->m_sLocation;

    switch (stat)
    {
        case UPnPResult_Success:
            LOG(VB_UPNP, LOG_INFO,
                QString("ConnectBackend() - success. New hostname: %1")
                .arg(m_dbParams->m_dbHostName));
            return true;

        case UPnPResult_HumanInterventionRequired:
            LOG(VB_GENERAL, LOG_ERR, QString("Need Human: %1").arg(message));
            ShowOkPopup(message);

            if (TryDBfromURL("", dev->m_sLocation))
                return true;

            break;

        case UPnPResult_ActionNotAuthorized:
            LOG(VB_GENERAL, LOG_ERR,
                QString("Access denied for %1. Wrong PIN?")
                .arg(backendName));
            PromptForPassword();
            break;

        default:
            LOG(VB_GENERAL, LOG_ERR,
                QString("GetConnectionInfo() failed for %1 : %2")
                .arg(backendName, message));
            ShowOkPopup(message);
    }

    // Back to the list, so the user can choose a different backend:
    SetFocusWidget(m_backendList);
    return false;
}

void BackendSelection::Cancel(void)
{
    CloseWithDecision(kCancelConfigure);
}

void BackendSelection::Load(void)
{
    SSDP::AddListener(this);
    SSDP::Instance()->PerformSearch(SSDP::kBackendURI);
}

void BackendSelection::Init(void)
{
    SSDPCacheEntries *pEntries = SSDPCache::Instance()->Find(SSDP::kBackendURI);
    if (pEntries)
    {
        EntryMap ourMap;
        pEntries->GetEntryMap(ourMap);
        pEntries->DecrRef();

        for (auto * devLoc : std::as_const(ourMap))
        {
            AddItem(devLoc);
            devLoc->DecrRef();
        }
    }
}

void BackendSelection::Manual(void)
{
    CloseWithDecision(kManualConfigure);
}

void BackendSelection::RemoveItem(const QString& USN_)
{
    m_mutex.lock();

    ItemMap::iterator it = m_devices.find(USN_);

    if (it != m_devices.end())
    {
        if (*it)
            (*it)->DecrRef();
        m_devices.erase(it);
    }

    m_mutex.unlock();
}

bool BackendSelection::TryDBfromURL(const QString &error, const QString& URL)
{
    if (ShowOkPopup(error + tr("Shall I attempt to connect to this"
                    " host with default database parameters?")))
    {
        static const QRegularExpression re {"http[s]?://([^:/]+)",
            QRegularExpression::CaseInsensitiveOption};
        QRegularExpressionMatch match = re.match(URL);
        if (match.hasMatch())
        {
            m_dbParams->m_dbHostName = match.captured(1);
            return true;
        }
    }

    return false;
}

void BackendSelection::customEvent(QEvent *event)
{
    if (event->type() == MythEvent::kMythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(event);
        if (me == nullptr)
            return;

        const QString&    message = me->Message();
        const QString&    URI     = me->ExtraData(0);
        const QString&    URN     = me->ExtraData(1);
        const QString&    URL     = me->ExtraData(2);


        LOG(VB_UPNP, LOG_DEBUG,
                 QString("BackendSelection::customEvent(%1, %2, %3, %4)")
                .arg(message, URI, URN, URL));

        if (message.startsWith("SSDP_ADD") &&
            URI.startsWith("urn:schemas-mythtv-org:device:MasterMediaServer:"))
        {
            DeviceLocation *devLoc = SSDP::Find(URI, URN);
            if (devLoc)
            {
                AddItem(devLoc);
                devLoc->DecrRef();
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
        auto *dce = dynamic_cast<DialogCompletionEvent*>(event);
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

    auto *pwDialog = new MythTextInputDialog(popupStack, message,
                                             FilterNone, true);

    if (pwDialog->Create())
    {
        pwDialog->SetReturnEvent(this, "password");
        popupStack->AddScreen(pwDialog);
    }
    else
    {
        delete pwDialog;
    }
}

void BackendSelection::Close(void)
{
    CloseWithDecision(kCancelConfigure);
}

void BackendSelection::CloseWithDecision(Decision d)
{
    m_backendDecision = d;

    if (m_exitOnFinish)
        m_loop->quit();
    else
        MythScreenType::Close();
}
