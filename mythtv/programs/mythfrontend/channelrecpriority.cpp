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

#include "rankchannels.h"
#include "infodialog.h"
#include "tv.h"

#include "dialogbox.h"
#include "mythcontext.h"
#include "infostructs.h"

RankChannels::RankChannels(QSqlDatabase *ldb, MythMainWindow *parent, 
                           const char *name)
            : MythDialog(parent, name)
{
    db = ldb;

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
    if (!theme->LoadTheme(xmldata, "rankchannels"))
    {
        DialogBox diag(gContext->GetMainWindow(), "The theme you are using "
                       "does not contain a 'rankchannels' element.  Please "
                       "contact the theme creator and ask if they could "
                       "please update it.<br><br>The next screen will be empty."
                       "  Escape out of it to return to the menu.");
        diag.AddButton("OK");
        diag.exec();

        return;
    }

    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("ranklist");
        if (ltype)
        {
            listsize = ltype->GetItems();
        }
    }
    else
    {
        cerr << "MythFrontEnd: ChannelRank - Failed to get selector object.\n";
        exit(0);
    }

    bgTransBackup = new QPixmap();
    resizeImage(bgTransBackup, "trans-backup.png");

    updateBackground();

    FillList();
    sortType = (SortType)gContext->GetNumSetting("ChannelRankSorting", 
												 (int)byChannel);

    SortList(); 
    inList = 0;
    inData = 0;
    setNoErase();

    gContext->addListener(this);
}

RankChannels::~RankChannels()
{
    gContext->removeListener(this);
    delete theme;
    if (bgTransBackup)
        delete bgTransBackup;
    if (curitem)
        delete curitem;
}

void RankChannels::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
        case Key_Up: cursorUp(); break;
        case Key_Down: cursorDown(); break;
        case Key_PageUp: case Key_3: pageUp(); break;
        case Key_PageDown: case Key_9: pageDown(); break;
        case Key_Right: changeRank(1); break;
        case Key_Left: changeRank(-1); break;
        case Key_Escape: saveRank(); break;
        case Key_1: if(sortType == byChannel)
                        break; 
                    sortType = byChannel; 
                    SortList();     
                    update(fullRect);
                    break;
        case Key_2: if(sortType == byRank)
                        break; 
                    sortType = byRank; 
                    SortList(); 
                    update(fullRect);
                    break;
        default: MythDialog::keyPressEvent(e); break;
    }
}

void RankChannels::LoadWindow(QDomElement &element)
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
                exit(0);
            }
        }
    }
}

void RankChannels::parseContainer(QDomElement &element)
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

void RankChannels::resizeImage(QPixmap *dst, QString file)
{
    QString baseDir = gContext->GetInstallPrefix();
    QString themeDir = gContext->FindThemeDir("");
    themeDir = themeDir + gContext->GetSetting("Theme") + "/";
    baseDir = baseDir + "/share/mythtv/themes/default/";

    QFile checkFile(themeDir + file);

    if (checkFile.exists())
        file = themeDir + file;
    else
        file = baseDir + file;

    if (hmult == 1 && wmult == 1)
    {
        dst->load(file);
    }
    else
    {
        QImage *sourceImg = new QImage();
        if (sourceImg->load(file))
        {
            QImage scalerImg;
            scalerImg = sourceImg->smoothScale((int)(sourceImg->width() * wmult),
                                               (int)(sourceImg->height() * hmult));
            dst->convertFromImage(scalerImg);
        }
        delete sourceImg;
    }
}

void RankChannels::updateBackground(void)
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

void RankChannels::paintEvent(QPaintEvent *e)
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

void RankChannels::cursorDown(bool page)
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

    if (inList >= listCount)
        inList = listCount - 1;

    update(fullRect);
}

void RankChannels::cursorUp(bool page)
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

void RankChannels::changeRank(int howMuch) 
{
    int tempRank, cnt;
    QPainter p(this);
    QMap<QString, ChannelInfo>::Iterator it;
    ChannelInfo *tempInfo;

    for (cnt=0, it = channelData.begin(); cnt < inList+inData; cnt++, ++it);

    tempInfo = &(it.data());

    tempRank = tempInfo->rank.toInt() + howMuch;
    tempInfo->rank = QString::number(tempRank);
    if (sortType == byRank)
        SortList();
    updateList(&p);
    updateInfo(&p);
}

void RankChannels::applyChannelRankChange(QSqlDatabase *db, QString chanid, 
                                          const QString &newrank) 
{
    QString query = QString("UPDATE channel SET rank = '%1' "
                            "WHERE chanid = '%2';").arg(newrank).arg(chanid);
    QSqlQuery result = db->exec(query);
}

void RankChannels::saveRank(void) 
{
    QMap<QString, ChannelInfo>::Iterator it;
    ChannelInfo *tempInfo;
    QString *tempRank, key;

    for (it = channelData.begin(); it != channelData.end(); ++it) 
    {
        tempInfo = &(it.data());
        key = QString::number(tempInfo->chanid);
        tempRank = &rankData[key];
        if(tempInfo->rank != *tempRank)
            applyChannelRankChange(db, QString::number(tempInfo->chanid), 
                                   tempInfo->rank);
    }

    gContext->SaveSetting("ChannelRankSorting", (int)sortType);
    done(MythDialog::Accepted);
}

void RankChannels::FillList(void)
{
    QString key;
    int cnt = 999;

    channelData.clear();

    QString query = QString("SELECT chanid, channum, callsign, icon, rank "
                            "FROM channel;");

    QSqlQuery result = db->exec(query);

    if (result.isActive() && result.numRowsAffected() > 0)
        while (result.next()) {
            ChannelInfo *chaninfo = new ChannelInfo;
            chaninfo->chanid = result.value(0).toInt();
            chaninfo->chanstr = result.value(1).toString();
            chaninfo->callsign = result.value(2).toString();
            chaninfo->iconpath = result.value(3).toString();
            chaninfo->rank = result.value(4).toString();
            channelData[QString::number(cnt)] = *chaninfo;
            rankData[QString::number(chaninfo->chanid)] = chaninfo->rank;
            cnt--;
            dataCount++;
        }
}

typedef struct RankInfo 
{
    ChannelInfo *chan;
    int cnt;
};

class channelSort 
{
    public:
        bool operator()(const RankInfo a, const RankInfo b) 
        {
            return(a.chan->chanstr.toInt() > b.chan->chanstr.toInt());
        }
};

class channelRankSort 
{
    public:
        bool operator()(const RankInfo a, const RankInfo b) 
        {
            if (a.chan->rank.toInt() == b.chan->rank.toInt())
                return (a.chan->chanstr.toInt() > b.chan->chanstr.toInt());
            return (a.chan->rank.toInt() < b.chan->rank.toInt());
        }
};

void RankChannels::SortList() 
{
    typedef vector<RankInfo> sortList;
    typedef QMap<QString, ChannelInfo> chanMap;
    
    int i, j;
    bool cursorChanged = false;
    sortList sortedList;
    chanMap::Iterator pit;
    sortList::iterator sit;
    ChannelInfo *chanInfo;
    RankInfo *rankInfo;
    chanMap sdCopy;
    QString cntStr = "";

    for (i = 0, pit = channelData.begin(); pit != channelData.end(); ++pit, i++)
    {
        chanInfo = &(pit.data());
        RankInfo tmp = {chanInfo, i};
        sortedList.push_back(tmp);
        sdCopy[pit.key()] = pit.data();
    }

    switch(sortType) 
    {
        case byChannel : sort(sortedList.begin(), sortedList.end(), 
                              channelSort());
                         break;
        case byRank : sort(sortedList.begin(), sortedList.end(), 
                           channelRankSort());
                      break;
    }

    channelData.clear();

    for(i = 0, sit = sortedList.begin(); sit != sortedList.end(); i++, ++sit ) 
    {
        rankInfo = &(*sit);

        for (j = 0, pit = sdCopy.begin(); j != rankInfo->cnt; j++, ++pit);

        chanInfo = &(pit.data());
        cntStr.sprintf("%d", 999-i);
        channelData[cntStr] = pit.data();

        if (!cursorChanged && rankInfo->cnt == inList+inData) 
        {
            inList = dataCount-i-1;
            if (inList > (int)((int)(listsize / 2) - 1)) 
            {
                inList = (int)(listsize / 2);
                inData = dataCount-i-1 - inList;
            }
            else
                inData = 0;
            if (inData > dataCount-listsize) 
            {
                inList += inData-(dataCount-listsize);
                inData = dataCount-listsize;
            }
            cursorChanged = true;
        }
    }
}

void RankChannels::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    
    int pastSkip = (int)inData;
    int rank;
    pageDowner = false;
    listCount = 0;

    typedef QMap<QString, ChannelInfo> RankData;
    ChannelInfo *tempInfo;
  
    RankData::Iterator it;
    RankData::Iterator start;
    RankData::Iterator end;

    start = channelData.begin();
    end = channelData.end();

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("ranklist");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            for (it = start; it != end; ++it)
            {
                if (cnt < listsize)
                {
                    if (pastSkip <= 0)
                    {
                        tempInfo = &(it.data());
                        rank = tempInfo->rank.toInt();

                        if (cnt == inList)
                        {
                            if (curitem)
                                delete curitem;
                            curitem = new ChannelInfo(*tempInfo);
                            ltype->SetItemCurrent(cnt);
                        }
                        
                        ltype->SetItemText(cnt, 1, tempInfo->chanstr);
                        ltype->SetItemText(cnt, 2, tempInfo->callsign);
                        if(tempInfo->rank.toInt() > 0)
                            ltype->SetItemText(cnt, 3, "+");
                        else if(tempInfo->rank.toInt() < 0)
                            ltype->SetItemText(cnt, 3, "-");
                        ltype->SetItemText(cnt, 4, QString::number(abs(rank)));

                        cnt++;
                        listCount++;
                    }
                    pastSkip--;
                }
                else
                    pageDowner = true;
            }
        }

        ltype->SetDownArrow(pageDowner);
        if (inData > 0)
            ltype->SetUpArrow(true);
        else
            ltype->SetUpArrow(false);
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

void RankChannels::updateInfo(QPainter *p)
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
                if(curitem->iconpath == "none" || curitem->iconpath == "")
                    curitem->iconpath = "blankicon.jpg";
                if (!curitem->icon)
                    curitem->LoadIcon(iconsize);
                if (curitem->icon)
                    itype->SetImage(*(curitem->icon));
            }

            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(curitem->callsign);
 
            type = (UITextType *)container->GetType("rank");
            if (type) {
                if(curitem->rank.toInt() > 0)
                    type->SetText("+"+curitem->rank);
                else
                    type->SetText(curitem->rank);
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
