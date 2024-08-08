// C++
#include <algorithm> // For std::sort()
#include <vector> // For std::vector

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/channelinfo.h"
#include "libmythtv/scheduledrecording.h"
#include "libmythtv/tv.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuitext.h"

// MythFrontend
#include "channelrecpriority.h"
#include "proglist.h"

struct RecPriorityInfo
{
    ChannelInfo *m_chan;
    int          m_cnt;
};

class channelSort
{
    public:
        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            if (a.m_chan->m_chanNum.toInt() == b.m_chan->m_chanNum.toInt())
                return(a.m_chan->m_sourceId > b.m_chan->m_sourceId);
            return(a.m_chan->m_chanNum.toInt() > b.m_chan->m_chanNum.toInt());
        }
};

class channelRecPrioritySort
{
    public:
        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b)
        {
            if (a.m_chan->m_recPriority == b.m_chan->m_recPriority)
                return (a.m_chan->m_chanNum.toInt() > b.m_chan->m_chanNum.toInt());
            return (a.m_chan->m_recPriority < b.m_chan->m_recPriority);
        }
};

ChannelRecPriority::ChannelRecPriority(MythScreenStack *parent)
                  : MythScreenType(parent, "ChannelRecPriority")
{
    m_sortType = (SortType)gCoreContext->GetNumSetting("ChannelRecPrioritySorting",
                                                 (int)byChannel);

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

    connect(m_channelList, &MythUIButtonList::itemSelected,
            this, &ChannelRecPriority::updateInfo);

    FillList();

    BuildFocusList();

    return true;
}

bool ChannelRecPriority::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("TV Frontend", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
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
        {
            handled = false;
        }
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

    auto *menuPopup = new MythDialogBox(label, popupStack, "chanrecmenupopup");

    if (!menuPopup->Create())
    {
        delete menuPopup;
        menuPopup = nullptr;
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

    auto *chanInfo = item->GetData().value<ChannelInfo *>();
    if (chanInfo == nullptr)
        return;

    // inc/dec recording priority
    int tempRecPriority = chanInfo->m_recPriority + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100)
    {
        chanInfo->m_recPriority = tempRecPriority;

        // order may change if sorting by recoring priority, so resort
        if (m_sortType == byRecPriority)
            SortList();
        else
        {
            item->SetText(QString::number(chanInfo->m_recPriority), "priority");
            updateInfo(item);
        }
    }
}

void ChannelRecPriority::applyChannelRecPriorityChange(const QString& chanid,
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
        QString key = QString::number(chanInfo->m_chanId);

        // if this channel's recording priority changed from when we entered
        // save new value out to db
        if (QString::number(chanInfo->m_recPriority) != m_origRecPriorityData[key])
            applyChannelRecPriorityChange(QString::number(chanInfo->m_chanId),
                                          QString::number(chanInfo->m_recPriority));
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
                   "icon, recpriority, name FROM channel "
                   "WHERE deleted IS NULL AND visible > 0");

    if (result.exec())
    {
        int cnt = 999;
        while (result.next())
        {
            auto *chaninfo = new ChannelInfo;
            chaninfo->m_chanId = result.value(0).toInt();
            chaninfo->m_chanNum = result.value(1).toString();
            chaninfo->m_sourceId = result.value(2).toInt();
            chaninfo->m_callSign = result.value(3).toString();
            QString iconurl = result.value(4).toString();
            if (!iconurl.isEmpty())
                iconurl = gCoreContext->GetMasterHostPrefix( "ChannelIcons", iconurl);
            chaninfo->m_icon = iconurl;
            chaninfo->m_recPriority = result.value(5).toInt();
            chaninfo->m_name = result.value(6).toString();

            chaninfo->SetSourceName(srcMap[chaninfo->m_sourceId]);

            m_channelData[QString::number(cnt)] = *chaninfo;

            // save recording priority value in map so we don't have to save
            // all channel's recording priority values when we exit
            m_origRecPriorityData[QString::number(chaninfo->m_chanId)] =
                QString::number(chaninfo->m_recPriority);

            cnt--;
        }
    }
    else if (!result.isActive())
    {
        MythDB::DBError("Get channel recording priorities query", result);
    }

    SortList();
}

void ChannelRecPriority::updateList()
{
    m_channelList->Reset();

    QMap<QString, ChannelInfo*>::Iterator it;
    for (it = m_sortedChannel.begin(); it != m_sortedChannel.end(); ++it)
    {
        ChannelInfo *chanInfo = *it;

        auto *item = new MythUIButtonListItem(m_channelList, "",
                                              QVariant::fromValue(chanInfo));

        QString fontState = "default";

        item->SetText(chanInfo->GetFormatted(ChannelInfo::kChannelLong),
                                             fontState);

        InfoMap infomap;
        chanInfo->ToMap(infomap);
        item->SetTextFromMap(infomap, fontState);

        item->DisplayState("normal", "status");

        if (!chanInfo->m_icon.isEmpty())
        {
            QString iconUrl = gCoreContext->GetMasterHostPrefix("ChannelIcons",
                                                                chanInfo->m_icon);
            item->SetImage(iconUrl, "icon");
            item->SetImage(iconUrl);
        }

        item->SetText(QString::number(chanInfo->m_recPriority), "priority", fontState);

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
        auto *channelItem = item->GetData().value<ChannelInfo *>();
        m_currentItem = channelItem;
    }

    int i = 0;
    int j = 0;
    std::vector<RecPriorityInfo> sortingList;
    QMap<QString, ChannelInfo>::iterator pit;
    std::vector<RecPriorityInfo>::iterator sit;

    // copy m_channelData into sortingList
    for (i = 0, pit = m_channelData.begin(); pit != m_channelData.end();
            ++pit, ++i)
    {
        ChannelInfo *chanInfo = &(*pit);
        RecPriorityInfo tmp = {chanInfo, i};
        sortingList.push_back(tmp);
    }

    switch(m_sortType)
    {
        case byRecPriority:
            std::sort(sortingList.begin(), sortingList.end(),
            channelRecPrioritySort());
            break;
        case byChannel:
        default:
            std::sort(sortingList.begin(), sortingList.end(),
            channelSort());
            break;
    }

    m_sortedChannel.clear();

    // rebuild m_channelData in sortingList order from m_sortedChannel
    for (i = 0, sit = sortingList.begin(); sit != sortingList.end(); i++, ++sit)
    {
        RecPriorityInfo *recPriorityInfo = &(*sit);

        // find recPriorityInfo[i] in m_sortedChannel
        for (j = 0, pit = m_channelData.begin(); j !=recPriorityInfo->m_cnt; j++, ++pit);

        m_sortedChannel[QString::number(999-i)] = &(*pit);
    }

    updateList();
}

void ChannelRecPriority::updateInfo(MythUIButtonListItem *item)
{
    if (!item)
        return;

    auto *channelItem = item->GetData().value<ChannelInfo *>();
    if (!m_channelData.isEmpty() && channelItem)
    {
        if (m_iconImage)
        {
            QString iconUrl = gCoreContext->GetMasterHostPrefix("ChannelIcons", channelItem->m_icon);
            m_iconImage->SetFilename(iconUrl);
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

    auto *chanInfo = item->GetData().value<ChannelInfo *>();

    if (!chanInfo || chanInfo->m_chanId < 1)
        return;

    QString chanID = QString("%1").arg(chanInfo->m_chanId);
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plChannel, chanID, "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}


void ChannelRecPriority::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        auto *dce = (DialogCompletionEvent*)(event);

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
