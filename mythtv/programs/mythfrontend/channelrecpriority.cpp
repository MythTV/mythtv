#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qsqldatabase.h>
#include <qdatetime.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qheader.h>

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
using namespace std;

#include "channelrecpriority.h"
#include "tv.h"

#include "dialogbox.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "scheduledrecording.h"
#include "infostructs.h"

ChannelRecPriority::ChannelRecPriority(MythMainWindow *parent, const char *name)
                  : MythDialog(parent, name)
{
    curitem = NULL;
    pageDowner = false;
    bgTransBackup = NULL;

    listCount = 0;
    dataCount = 0;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, "recprioritychannels"))
    {
        DialogBox diag(gContext->GetMainWindow(), tr("The theme you are using "
                       "does not contain a 'recprioritychannels' element.  "
                       "Please contact the theme creator and ask if they could "
                       "please update it.<br><br>The next screen will be empty."
                       "  Escape out of it to return to the menu."));
        diag.AddButton(tr("OK"));
        diag.exec();

        return;
    }

    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("recprioritylist");
        if (ltype)
        {
            listsize = ltype->GetItems();
        }
    }
    else
    {
        cerr << "MythFrontEnd: ChannelRecPriority - Failed to get selector.\n";
        exit(19);
    }

    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    updateBackground();

    FillList();
    sortType = (SortType)gContext->GetNumSetting("ChannelRecPrioritySorting",
                                                 (int)byChannel);

    SortList(); 
    inList = 0;
    inData = 0;
    setNoErase();

    longchannelformat = 
        gContext->GetSetting("LongChannelFormat", "<num> <name>");

    gContext->addListener(this);
}

ChannelRecPriority::~ChannelRecPriority()
{
    gContext->removeListener(this);
    delete theme;
    if (bgTransBackup)
        delete bgTransBackup;
    if (curitem)
        delete curitem;
}

void ChannelRecPriority::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                cursorUp();
            else if (action == "DOWN")
                cursorDown();
            else if (action == "PAGEUP")
                pageUp();
            else if (action == "PAGEDOWN")
                pageDown();
            else if (action == "RIGHT")
                changeRecPriority(1);
            else if (action == "LEFT")
                changeRecPriority(-1);
            else if (action == "ESCAPE")
            {
                saveRecPriority(); 
                gContext->SaveSetting("ChannelRecPrioritySorting",
                                      (int)sortType);
                done(MythDialog::Accepted);
            }
            else if (action == "1")
            {
                if (sortType != byChannel)
                {
                    sortType = byChannel; 
                    SortList();     
                    update(fullRect);
                }
            }
            else if (action == "2")
            {
                if (sortType != byRecPriority)
                {
                    sortType = byRecPriority; 
                    SortList(); 
                    update(fullRect);
                }
            }
            else if (action == "PREVVIEW" || action == "NEXTVIEW")
            {
                if (sortType == byChannel)
                    sortType = byRecPriority;
                else
                    sortType = byChannel; 
                SortList();     
                update(fullRect);
            }
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void ChannelRecPriority::LoadWindow(QDomElement &element)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else
            {
                cerr << "Unknown element: " << e.tagName() << endl;
                exit(20);
            }
        }
    }
}

void ChannelRecPriority::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        listRect = area;
    if (name.lower() == "channel_info")
        infoRect = area;
}

void ChannelRecPriority::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
        container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}

void ChannelRecPriority::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);
 
    if (r.intersects(listRect))
    {
        updateList(&p);
    }
    if (r.intersects(infoRect))
    {
        updateInfo(&p);
    }
}

void ChannelRecPriority::cursorDown(bool page)
{
    if (page == false)
    {
        if (inList > (int)((int)(listsize / 2) - 1)
            && ((int)(inData + listsize) <= (int)(dataCount - 1))
            && pageDowner == true)
        {
            inData++;
            inList = (int)(listsize / 2);
        }
        else
        {
            inList++;

            if (inList >= listCount)
                inList = listCount - 1;
        }
    }
    else if (page == true && pageDowner == true)
    {
        if (inList >= (int)(listsize / 2) || inData != 0)
        {
            inData = inData + listsize;
        }
        else if (inList < (int)(listsize / 2) && inData == 0)
        {
            inData = (int)(listsize / 2) + inList;
            inList = (int)(listsize / 2);
        }
    }
    else if (page == true && pageDowner == false)
    {
        inList = listsize - 1;
    }

    if ((int)(inData + inList) >= (int)(dataCount))
    {
        inData = dataCount - listsize;
        inList = listsize - 1;
    }
    else if ((int)(inData + listsize) >= (int)dataCount)
    {
        inData = dataCount - listsize;
    }

    if (inData < 0) 
        inData = 0;

    if (inList >= listCount)
        inList = listCount - 1;

    update(fullRect);
}

void ChannelRecPriority::cursorUp(bool page)
{
    if (page == false)
    {
        if (inList < ((int)(listsize / 2) + 1) && inData > 0)
        {
            inList = (int)(listsize / 2);
            inData--;
            if (inData < 0)
            {
                inData = 0;
                inList--;
            }
         }
         else
         {
             inList--;
         }
     }
     else if (page == true && inData > 0)
     {
         inData = inData - listsize;
         if (inData < 0)
         {
             inList = inList + inData;
             inData = 0;
             if (inList < 0)
                 inList = 0;
         }

         if (inList > (int)(listsize / 2))
         {
             inList = (int)(listsize / 2);
             inData = inData + (int)(listsize / 2) - 1;
         }
     }
     else if (page == true)
     {
         inData = 0;
         inList = 0;
     }

     if (inList > -1)
     {
         update(fullRect);
     }
     else
         inList = 0;
}

void ChannelRecPriority::changeRecPriority(int howMuch) 
{
    int tempRecPriority, cnt;
    QPainter p(this);
    QMap<QString, ChannelInfo>::Iterator it;
    ChannelInfo *chanInfo;

    // iterate through channelData till we hit the line where
    // the cursor currently is
    for (cnt=0, it = channelData.begin(); cnt < inList+inData; cnt++, ++it);

    chanInfo = &(it.data());

    // inc/dec recording priority
    tempRecPriority = chanInfo->recpriority.toInt() + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100)
    {
        chanInfo->recpriority = QString::number(tempRecPriority);

        // order may change if sorting by recoring priority, so resort
        if (sortType == byRecPriority)
            SortList();
        updateList(&p);
        updateInfo(&p);
    }
}

void ChannelRecPriority::applyChannelRecPriorityChange(QString chanid, 
                                                 const QString &newrecpriority) 
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE channel SET recpriority = :RECPRI "
                            "WHERE chanid = CHANID ;");
    query.bindValue(":RECPRI", newrecpriority);
    query.bindValue(":CHANID", chanid);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Save recpriority update", query);
}

void ChannelRecPriority::saveRecPriority(void) 
{
    QMap<QString, ChannelInfo>::Iterator it;

    for (it = channelData.begin(); it != channelData.end(); ++it) 
    {
        ChannelInfo *chanInfo = &(it.data());
        QString key = QString::number(chanInfo->chanid);

        // if this channel's recording priority changed from when we entered
        // save new value out to db
        if (chanInfo->recpriority != origRecPriorityData[key])
            applyChannelRecPriorityChange(QString::number(chanInfo->chanid),
                                          chanInfo->recpriority);
    }
    ScheduledRecording::signalChange(0);
}

void ChannelRecPriority::FillList(void)
{
    int cnt = 999;

    channelData.clear();

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT chanid, channum, sourceid, callsign, "
                            "icon, recpriority, name FROM channel;");

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        while (result.next()) {
            ChannelInfo *chaninfo = new ChannelInfo;
            chaninfo->chanid = result.value(0).toInt();
            chaninfo->chanstr = result.value(1).toString();
            chaninfo->sourceid = result.value(2).toInt();
            chaninfo->callsign = QString::fromUtf8(result.value(3).toString());
            chaninfo->iconpath = result.value(4).toString();
            chaninfo->recpriority = result.value(5).toString();
            chaninfo->channame = QString::fromUtf8(result.value(6).toString());
            channelData[QString::number(cnt)] = *chaninfo;

            // save recording priority value in map so we don't have to save 
            // all channel's recording priority values when we exit
            origRecPriorityData[QString::number(chaninfo->chanid)] = 
                    chaninfo->recpriority;

            cnt--;
            dataCount++;
        }
    }
    else if (!result.isActive())
        MythContext::DBError("Get channel recording priorities query", result);
}

typedef struct RecPriorityInfo 
{
    ChannelInfo *chan;
    int cnt;
};

class channelSort 
{
    public:
        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            return(a.chan->chanstr.toInt() > b.chan->chanstr.toInt());
        }
};

class channelRecPrioritySort 
{
    public:
        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            if (a.chan->recpriority.toInt() == b.chan->recpriority.toInt())
                return (a.chan->chanstr.toInt() > b.chan->chanstr.toInt());
            return (a.chan->recpriority.toInt() < b.chan->recpriority.toInt());
        }
};

void ChannelRecPriority::SortList() 
{
    typedef vector<RecPriorityInfo> sortList;
    typedef QMap<QString, ChannelInfo> chanMap;
    
    int i, j;
    bool cursorChanged = false;
    sortList sortedList;
    chanMap::Iterator pit;
    sortList::iterator sit;
    ChannelInfo *chanInfo;
    RecPriorityInfo *recPriorityInfo;
    chanMap cdCopy;

    // copy channelData into sortedList and make a copy
    // of channelData in cdCopy
    for (i = 0, pit = channelData.begin(); pit != channelData.end(); ++pit, i++)
    {
        chanInfo = &(pit.data());
        RecPriorityInfo tmp = {chanInfo, i};
        sortedList.push_back(tmp);
        cdCopy[pit.key()] = pit.data();
    }

    switch(sortType) 
    {
        case byRecPriority:
            sort(sortedList.begin(), sortedList.end(), 
            channelRecPrioritySort());
            break;
        case byChannel:
        default:
            sort(sortedList.begin(), sortedList.end(), 
            channelSort());
            break;
    }

    channelData.clear();

    // rebuild channelData in sortedList order from cdCopy
    for(i = 0, sit = sortedList.begin(); sit != sortedList.end(); i++, ++sit ) 
    {
        recPriorityInfo = &(*sit);

        // find recPriorityInfo[i] in cdCopy
        for (j = 0, pit = cdCopy.begin(); j !=recPriorityInfo->cnt; j++, ++pit);

        chanInfo = &(pit.data());

        // put back into channelData
        channelData[QString::number(999-i)] = pit.data();

        // if recPriorityInfo[i] is the channel where the cursor
        // was pre-sort then we need to update to cursor
        // to the ith position
        if (!cursorChanged && recPriorityInfo->cnt == inList+inData) 
        {
            inList = dataCount - i - 1;
            if (inList > (int)((int)(listsize / 2) - 1)) 
            {
                inList = (int)(listsize / 2);
                inData = dataCount - i - 1 - inList;
            }
            else
                inData = 0;

            if (dataCount > listsize && inData > dataCount - listsize) 
            {
                inList += inData - (dataCount - listsize);
                inData = dataCount - listsize;
            }
            cursorChanged = true;
        }
    }
}

void ChannelRecPriority::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    
    int pastSkip = (int)inData;
    pageDowner = false;
    listCount = 0;

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("recprioritylist");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            QMap<QString, ChannelInfo>::Iterator it;
            for (it = channelData.begin(); it != channelData.end(); ++it)
            {
                if (cnt < listsize)
                {
                    if (pastSkip <= 0)
                    {
                        ChannelInfo *chanInfo = &(it.data());
                        int recPriority = chanInfo->recpriority.toInt();

                        if (cnt == inList)
                        {
                            if (curitem)
                                delete curitem;
                            curitem = new ChannelInfo(*chanInfo);
                            ltype->SetItemCurrent(cnt);
                        }

                        ltype->SetItemText(cnt, 1, 
                                           chanInfo->Text(longchannelformat));

                        if (chanInfo->recpriority.toInt() > 0)
                            ltype->SetItemText(cnt, 2, "+");
                        else if (chanInfo->recpriority.toInt() < 0)
                            ltype->SetItemText(cnt, 2, "-");
                        ltype->SetItemText(cnt, 3, 
                                           QString::number(abs(recPriority)));

                        cnt++;
                        listCount++;
                    }
                    pastSkip--;
                }
                else
                    pageDowner = true;
            }

            ltype->SetDownArrow(pageDowner);
            if (inData > 0)
                ltype->SetUpArrow(true);
            else
                ltype->SetUpArrow(false);
        }
    }

    if (channelData.count() <= 0)
        container = theme->GetSet("norecordings_list");

    if (container)
    {
       container->Draw(&tmp, 0, 0);
       container->Draw(&tmp, 1, 0);
       container->Draw(&tmp, 2, 0);
       container->Draw(&tmp, 3, 0);
       container->Draw(&tmp, 4, 0);
       container->Draw(&tmp, 5, 0);
       container->Draw(&tmp, 6, 0);
       container->Draw(&tmp, 7, 0);
       container->Draw(&tmp, 8, 0);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ChannelRecPriority::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    QString rectype;
    UIImageType *itype = NULL;

    if (channelData.count() > 0 && curitem)
    {  
        LayerSet *container = NULL;
        container = theme->GetSet("channel_info");
        if (container)
        {
            itype = (UIImageType *)container->GetType("icon");
            if (itype) {
                int iconsize = itype->GetSize();
                if (curitem->iconpath == "none" || curitem->iconpath == "")
                    curitem->iconpath = "blankicon.jpg";
                if (!curitem->icon)
                    curitem->LoadIcon(iconsize);
                if (curitem->icon)
                    itype->SetImage(*(curitem->icon));
            }

            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(curitem->Text(longchannelformat));
 
            type = (UITextType *)container->GetType("source");
            if (type)
                type->SetText(QString("%1").arg(curitem->sourceid));

            type = (UITextType *)container->GetType("recpriority");
            if (type) {
                if (curitem->recpriority.toInt() > 0)
                    type->SetText("+"+curitem->recpriority);
                else
                    type->SetText(curitem->recpriority);
            }
        }
       
        if (container)
        {
            container->Draw(&tmp, 4, 0);
            container->Draw(&tmp, 5, 0);
            container->Draw(&tmp, 6, 0);
            container->Draw(&tmp, 7, 0);
            container->Draw(&tmp, 8, 0);
        }
    }
    else
    {
        LayerSet *norec = theme->GetSet("norecordings_info");
        if (norec)
        {
            norec->Draw(&tmp, 4, 0);
            norec->Draw(&tmp, 5, 0);
            norec->Draw(&tmp, 6, 0);
            norec->Draw(&tmp, 7, 0);
            norec->Draw(&tmp, 8, 0);
        }

    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}
