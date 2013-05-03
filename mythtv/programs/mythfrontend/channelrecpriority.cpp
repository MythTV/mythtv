
#include "channelrecpriority.h"

#include <algorithm> // For std::sort()
#include <vector> // For std::vector
using namespace std;

#include "tv.h"

#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "scheduledrecording.h"
#include "proglist.h"
#include "channelinfo.h"

#include "mythuihelper.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythuibuttonlist.h"
#include "mythdialogbox.h"
#include "mythmainwindow.h"

typedef struct RecPriorityInfo
{
    ChannelInfo *chan;
    int cnt;
} RecPriorityInfo;

class channelSort
{
    public:
        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            if (a.chan->channum.toInt() == b.chan->channum.toInt())
                return(a.chan->sourceid > b.chan->sourceid);
            return(a.chan->channum.toInt() > b.chan->channum.toInt());
        }
};

class channelRecPrioritySort
{
    public:
        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            if (a.chan->recpriority == b.chan->recpriority)
                return (a.chan->channum.toInt() > b.chan->channum.toInt());
            return (a.chan->recpriority < b.chan->recpriority);
        }
};

ChannelRecPriority::ChannelRecPriority(MythScreenStack *parent)
                  : MythScreenType(parent, "ChannelRecPriority"),
                    m_channelList(NULL), m_chanstringText(NULL),
                    m_channameText(NULL), m_channumText(NULL),
                    m_callsignText(NULL), m_sourcenameText(NULL),
                    m_sourceidText(NULL), m_priorityText(NULL),
                    m_iconImage(NULL)
{
    m_sortType = (SortType)gCoreContext->GetNumSetting("ChannelRecPrioritySorting",
                                                 (int)byChannel);

    m_currentItem = NULL;

    gCoreContext->addListener(this);
}

ChannelRecPriority::~ChannelRecPriority()
{
    saveRecPriority();
    gCoreContext->SaveSetting("ChannelRecPrioritySorting",
                            (int)m_sortType);
    gCoreContext->removeListener(this);
}

bool ChannelRecPriority::Create()
{
    if (!LoadWindowFromXML("schedule-ui.xml", "channelrecpriority", this))
        return false;

    m_channelList = dynamic_cast<MythUIButtonList *> (GetChild("channels"));

    m_chanstringText = dynamic_cast<MythUIText *> (GetChild("chanstring")); // Deprecated, use mapped text
    m_priorityText = dynamic_cast<MythUIText *> (GetChild("priority"));

    m_iconImage = dynamic_cast<MythUIImage *> (GetChild("icon"));

    if (!m_channelList)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    connect(m_channelList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(updateInfo(MythUIButtonListItem*)));

    FillList();

    BuildFocusList();

    return true;
}

bool ChannelRecPriority::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UPCOMING")
            upcoming();
        else if (action == "RANKINC")
            changeRecPriority(1);
        else if (action == "RANKDEC")
            changeRecPriority(-1);
        else if (action == "1")
        {
            if (m_sortType != byChannel)
            {
                m_sortType = byChannel;
                SortList();
            }
        }
        else if (action == "2")
        {
            if (m_sortType != byRecPriority)
            {
                m_sortType = byRecPriority;
                SortList();
            }
        }
        else if (action == "PREVVIEW" || action == "NEXTVIEW")
        {
            if (m_sortType == byChannel)
                m_sortType = byRecPriority;
            else
                m_sortType = byChannel;
            SortList();
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void ChannelRecPriority::ShowMenu()
{
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    QString label = tr("Channel Options");

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythDialogBox *menuPopup = new MythDialogBox(label, popupStack,
                                                 "chanrecmenupopup");

    if (!menuPopup->Create())
    {
        delete menuPopup;
        menuPopup = NULL;
        return;
    }

    menuPopup->SetReturnEvent(this, "options");

    menuPopup->AddButton(tr("Program List"));
    //menuPopup->AddButton(tr("Reset All Priorities"));

    popupStack->AddScreen(menuPopup);
}

void ChannelRecPriority::changeRecPriority(int howMuch)
{
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    ChannelInfo *chanInfo = qVariantValue<ChannelInfo *>(item->GetData());

    // inc/dec recording priority
    int tempRecPriority = chanInfo->recpriority + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100)
    {
        chanInfo->recpriority = tempRecPriority;

        // order may change if sorting by recoring priority, so resort
        if (m_sortType == byRecPriority)
            SortList();
        else
        {
            item->SetText(QString::number(chanInfo->recpriority), "priority");
            updateInfo(item);
        }
    }
}

void ChannelRecPriority::applyChannelRecPriorityChange(QString chanid,
                                                 const QString &newrecpriority)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE channel SET recpriority = :RECPRI "
                            "WHERE chanid = :CHANID ;");
    query.bindValue(":RECPRI", newrecpriority);
    query.bindValue(":CHANID", chanid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("Save recpriority update", query);
}

void ChannelRecPriority::saveRecPriority(void)
{
    QMap<QString, ChannelInfo>::Iterator it;

    for (it = m_channelData.begin(); it != m_channelData.end(); ++it)
    {
        ChannelInfo *chanInfo = &(*it);
        QString key = QString::number(chanInfo->chanid);

        // if this channel's recording priority changed from when we entered
        // save new value out to db
        if (QString::number(chanInfo->recpriority) != m_origRecPriorityData[key])
            applyChannelRecPriorityChange(QString::number(chanInfo->chanid),
                                          QString::number(chanInfo->recpriority));
    }
    ScheduledRecording::ReschedulePlace("SaveChannelPriority");
}

void ChannelRecPriority::FillList(void)
{
    QMap<int, QString> srcMap;

    m_channelData.clear();
    m_sortedChannel.clear();

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT sourceid, name FROM videosource;");

    if (result.exec())
    {
        while (result.next())
        {
            srcMap[result.value(0).toInt()] = result.value(1).toString();
        }
    }
    result.prepare("SELECT chanid, channum, sourceid, callsign, "
                   "icon, recpriority, name FROM channel WHERE visible=1;");

    if (result.exec())
    {
        int cnt = 999;
        while (result.next())
        {
            ChannelInfo *chaninfo = new ChannelInfo;
            chaninfo->chanid = result.value(0).toInt();
            chaninfo->channum = result.value(1).toString();
            chaninfo->sourceid = result.value(2).toInt();
            chaninfo->callsign = result.value(3).toString();
            chaninfo->icon = result.value(4).toString();
            chaninfo->recpriority = result.value(5).toInt();
            chaninfo->name = result.value(6).toString();

            chaninfo->SetSourceName(srcMap[chaninfo->sourceid]);

            m_channelData[QString::number(cnt)] = *chaninfo;

            // save recording priority value in map so we don't have to save
            // all channel's recording priority values when we exit
            m_origRecPriorityData[QString::number(chaninfo->chanid)] =
                    chaninfo->recpriority;

            cnt--;
        }
    }
    else if (!result.isActive())
        MythDB::DBError("Get channel recording priorities query", result);

    SortList();
}

void ChannelRecPriority::updateList()
{
    m_channelList->Reset();

    QMap<QString, ChannelInfo*>::Iterator it;
    MythUIButtonListItem *item;
    for (it = m_sortedChannel.begin(); it != m_sortedChannel.end(); ++it)
    {
        ChannelInfo *chanInfo = *it;

        item = new MythUIButtonListItem(m_channelList, "",
                                                   qVariantFromValue(chanInfo));

        QString fontState = "default";

        item->SetText(chanInfo->GetFormatted(ChannelInfo::kChannelLong),
                                             fontState);

        InfoMap infomap;
        chanInfo->ToMap(infomap);
        item->SetTextFromMap(infomap, fontState);

        item->DisplayState("normal", "status");

        item->SetImage(chanInfo->icon, "icon");
        item->SetImage(chanInfo->icon);

        item->SetText(QString::number(chanInfo->recpriority), "priority", fontState);

        if (m_currentItem == chanInfo)
            m_channelList->SetItemCurrent(item);
    }

    // this textarea name is depreciated use 'nochannels_warning' instead
    MythUIText *noChannelsText = dynamic_cast<MythUIText*>(GetChild("norecordings_info"));

    if (!noChannelsText)
        noChannelsText = dynamic_cast<MythUIText*>(GetChild("nochannels_warning"));

    if (noChannelsText)
        noChannelsText->SetVisible(m_channelData.isEmpty());
}


void ChannelRecPriority::SortList()
{
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();

    if (item)
    {
        ChannelInfo *channelItem = qVariantValue<ChannelInfo *>(item->GetData());
        m_currentItem = channelItem;
    }

    int i, j;
    vector<RecPriorityInfo> sortingList;
    QMap<QString, ChannelInfo>::iterator pit;
    vector<RecPriorityInfo>::iterator sit;
    ChannelInfo *chanInfo;
    RecPriorityInfo *recPriorityInfo;

    // copy m_channelData into sortingList
    for (i = 0, pit = m_channelData.begin(); pit != m_channelData.end();
            ++pit, ++i)
    {
        chanInfo = &(*pit);
        RecPriorityInfo tmp = {chanInfo, i};
        sortingList.push_back(tmp);
    }

    switch(m_sortType)
    {
        case byRecPriority:
            sort(sortingList.begin(), sortingList.end(),
            channelRecPrioritySort());
            break;
        case byChannel:
        default:
            sort(sortingList.begin(), sortingList.end(),
            channelSort());
            break;
    }

    m_sortedChannel.clear();

    // rebuild m_channelData in sortingList order from m_sortedChannel
    for (i = 0, sit = sortingList.begin(); sit != sortingList.end(); i++, ++sit)
    {
        recPriorityInfo = &(*sit);

        // find recPriorityInfo[i] in m_sortedChannel
        for (j = 0, pit = m_channelData.begin(); j !=recPriorityInfo->cnt; j++, ++pit);

        m_sortedChannel[QString::number(999-i)] = &(*pit);
    }

    updateList();
}

void ChannelRecPriority::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    ChannelInfo *channelItem = qVariantValue<ChannelInfo *>(item->GetData());
    if (!m_channelData.isEmpty() && channelItem)
    {
        QString rectype;
        if (m_iconImage)
        {
            m_iconImage->SetFilename(channelItem->icon);
            m_iconImage->Load();
        }

        InfoMap chanmap;
        channelItem->ToMap(chanmap);
        SetTextFromMap(chanmap);

        if (m_chanstringText)
            m_chanstringText->SetText(channelItem->GetFormatted(ChannelInfo::kChannelLong));
    }

    MythUIText *norecordingText = dynamic_cast<MythUIText*>
                                                (GetChild("norecordings_info"));

    if (norecordingText)
        norecordingText->SetVisible(m_channelData.isEmpty());
}

void ChannelRecPriority::upcoming()
{
    MythUIButtonListItem *item = m_channelList->GetItemCurrent();

    if (!item)
        return;

    ChannelInfo *chanInfo = qVariantValue<ChannelInfo *>(item->GetData());

    if (!chanInfo || chanInfo->chanid < 1)
        return;

    QString chanID = QString("%1").arg(chanInfo->chanid);
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plChannel, chanID, "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}


void ChannelRecPriority::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid  = dce->GetId();
        int     buttonnum = dce->GetResult();

        if (resultid == "options")
        {
            switch (buttonnum)
            {
                case 0:
                    upcoming();
                    break;
            }
        }
    }
}

