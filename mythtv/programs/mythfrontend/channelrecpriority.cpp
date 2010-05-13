
#include "channelrecpriority.h"

#include <algorithm> // For std::sort()
#include <vector> // For std::vector
using namespace std;

#include "tv.h"

#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythverbose.h"
#include "scheduledrecording.h"
#include "proglist.h"
#include "dbchannelinfo.h"

#include "mythuihelper.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythuibuttonlist.h"
#include "mythdialogbox.h"

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
            if (a.chan->chanstr.toInt() == b.chan->chanstr.toInt())
                return(a.chan->sourceid > b.chan->sourceid);
            return(a.chan->chanstr.toInt() > b.chan->chanstr.toInt());
        }
};

class channelRecPrioritySort
{
    public:
        bool operator()(const RecPriorityInfo &a, const RecPriorityInfo &b)
        {
            if (a.chan->recpriority.toInt() == b.chan->recpriority.toInt())
                return (a.chan->chanstr.toInt() > b.chan->chanstr.toInt());
            return (a.chan->recpriority.toInt() < b.chan->recpriority.toInt());
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

    m_longchannelformat =
        gCoreContext->GetSetting("m_longchannelformat", "<num> <name>");

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

    m_chanstringText = dynamic_cast<MythUIText *> (GetChild("chanstring"));
    m_channameText = dynamic_cast<MythUIText *> (GetChild("channame"));
    m_channumText = dynamic_cast<MythUIText *> (GetChild("chanstring"));
    m_callsignText = dynamic_cast<MythUIText *> (GetChild("callsign"));
    m_sourcenameText = dynamic_cast<MythUIText *> (GetChild("sourcename"));
    m_sourceidText = dynamic_cast<MythUIText *> (GetChild("sourceid"));
    m_priorityText = dynamic_cast<MythUIText *> (GetChild("priority"));

    m_iconImage = dynamic_cast<MythUIImage *> (GetChild("icon"));

    if (!m_channelList)
    {
        VERBOSE(VB_IMPORTANT, "Theme is missing critical theme elements.");
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
    int tempRecPriority = chanInfo->recpriority.toInt() + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100)
    {
        chanInfo->recpriority = QString::number(tempRecPriority);

        // order may change if sorting by recoring priority, so resort
        if (m_sortType == byRecPriority)
            SortList();
        else
        {
            item->SetText(chanInfo->recpriority, "priority");
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
        if (chanInfo->recpriority != m_origRecPriorityData[key])
            applyChannelRecPriorityChange(QString::number(chanInfo->chanid),
                                          chanInfo->recpriority);
    }
    ScheduledRecording::signalChange(0);
}

void ChannelRecPriority::FillList(void)
{
    QMap<int, QString> srcMap;

    m_channelData.clear();
    m_sortedChannel.clear();
    m_visMap.clear();

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
                   "icon, recpriority, name, visible FROM channel;");

    if (result.exec())
    {
        int cnt = 999;
        while (result.next())
        {
            ChannelInfo *chaninfo = new ChannelInfo;
            chaninfo->chanid = result.value(0).toInt();
            chaninfo->chanstr = result.value(1).toString();
            chaninfo->sourceid = result.value(2).toInt();
            chaninfo->callsign = result.value(3).toString();
            chaninfo->iconpath = result.value(4).toString();
            chaninfo->recpriority = result.value(5).toString();
            chaninfo->channame = result.value(6).toString();
            if (result.value(7).toInt() > 0)
                m_visMap[chaninfo->chanid] = true;
            chaninfo->sourcename = srcMap[chaninfo->sourceid];

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
        if (!m_visMap[chanInfo->chanid])
            fontState = "disabled";

        QString stringFormat = item->GetText();
        if (stringFormat.isEmpty())
            stringFormat = "<num>  <sign>  \"<name>\"";
        item->SetText(chanInfo->GetFormatted(stringFormat), fontState);

        item->SetText(chanInfo->chanstr, "channum", fontState);
        item->SetText(chanInfo->callsign, "callsign", fontState);
        item->SetText(chanInfo->channame, "name", fontState);
        item->SetText(QString().setNum(chanInfo->sourceid), "sourceid",
                        fontState);
        item->SetText(chanInfo->sourcename, "sourcename", fontState);
        if (m_visMap[chanInfo->chanid])
            item->DisplayState("normal", "status");
        else
            item->DisplayState("disabled", "status");

        item->SetImage(chanInfo->iconpath, "icon");
        item->SetImage(chanInfo->iconpath);

        item->SetText(chanInfo->recpriority, "priority", fontState);

        if (m_currentItem == chanInfo)
            m_channelList->SetItemCurrent(item);
    }

    MythUIText *norecordingText = dynamic_cast<MythUIText*>
                                                (GetChild("norecordings_info"));

    if (norecordingText)
        norecordingText->SetVisible(m_channelData.isEmpty());
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
            m_iconImage->SetFilename(channelItem->iconpath);
            m_iconImage->Load();
        }

        if (m_chanstringText)
            m_chanstringText->SetText(channelItem->GetFormatted(m_longchannelformat));
        if (m_callsignText)
            m_callsignText->SetText(channelItem->callsign);
        if (m_channumText)
            m_channumText->SetText(channelItem->chanstr);
        if (m_channameText)
            m_channameText->SetText(channelItem->channame);
        if (m_sourcenameText)
            m_sourcenameText->SetText(channelItem->sourcename);
        if (m_sourceidText)
            m_sourceidText->SetText(QString().setNum(channelItem->sourceid));
        if (m_priorityText)
            m_priorityText->SetText(channelItem->recpriority);
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

