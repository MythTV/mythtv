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

#include "rankprograms.h"
#include "scheduledrecording.h"
#include "infodialog.h"
#include "tv.h"

#include "dialogbox.h"
#include "mythcontext.h"
#include "remoteutil.h"

RankPrograms::RankPrograms(QSqlDatabase *ldb, MythMainWindow *parent, 
                             const char *name)
            : MythDialog(parent, name)
{
    db = ldb;

    doingSel = false;

    curitem = NULL;
    bgTransBackup = NULL;
    pageDowner = false;

    allowKeys = true;

    listCount = 0;
    dataCount = 0;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, "rankprograms"))
    {
        DialogBox diag(gContext->GetMainWindow(), "The theme you are using "
                       "does not contain a 'rankprograms' element.  Please "
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
        cerr << "MythFrontEnd: RankPrograms - Failed to get selector object.\n";
        exit(0);
    }

    bgTransBackup = new QPixmap();
    resizeImage(bgTransBackup, "trans-backup.png");

    updateBackground();

    FillList();
    sortType = (SortType)gContext->GetNumSetting("ProgramRankSorting", 
												 (int)byTitle);

    SortList(); 
    inList = 0;
    inData = 0;
    setNoErase();

    gContext->addListener(this);
}

RankPrograms::~RankPrograms()
{
    gContext->removeListener(this);
    delete theme;
    if (bgTransBackup)
        delete bgTransBackup;
    if (curitem)
        delete curitem;
}

void RankPrograms::keyPressEvent(QKeyEvent *e)
{
    if (!allowKeys)
        return;

    switch (e->key())
    {
        case Key_Up: cursorUp(); break;
        case Key_Down: cursorDown(); break;
        case Key_PageUp: case Key_3: pageUp(); break;
        case Key_PageDown: case Key_9: pageDown(); break;
        case Key_Right: changeRank(1); break;
        case Key_Left: changeRank(-1); break;
        case Key_Escape: saveRank(); break;
        case Key_1: if (sortType == byTitle)
                        break; 
                    sortType = byTitle; 
                    SortList();     
                    update(fullRect);
                    break;
        case Key_2: if (sortType == byRank)
                        break; 
                    sortType = byRank; 
                    SortList(); 
                    update(fullRect);
                    break;
        case Key_I: edit(); break;
        default: MythDialog::keyPressEvent(e); break;
    }
}

void RankPrograms::LoadWindow(QDomElement &element)
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

void RankPrograms::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "selector")
        listRect = area;
    if (name.lower() == "program_info")
        infoRect = area;
}

void RankPrograms::resizeImage(QPixmap *dst, QString file)
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

void RankPrograms::updateBackground(void)
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

void RankPrograms::paintEvent(QPaintEvent *e)
{
    if (doingSel)
        return;

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

void RankPrograms::cursorDown(bool page)
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

void RankPrograms::cursorUp(bool page)
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

void RankPrograms::edit(void)
{
    if (doingSel)
        return;

    if (!curitem)
        return;

    doingSel = true;

    ProgramInfo *rec = curitem;

    MythContext::KickDatabase(db);

    if (rec)
    {
        if (rec->GetProgramRecordingStatus(db) > ScheduledRecording::AllRecord)
        {
            ScheduledRecording record;
            record.loadByProgram(db, rec);
            record.exec(db);
        }
        else
        {
            InfoDialog diag(rec, gContext->GetMainWindow(), "Program Info");
            diag.exec();
        }

        ScheduledRecording::signalChange(db);

        FillList();
        update(fullRect);
    }

    doingSel = false;
}

void RankPrograms::changeRank(int howMuch) 
{
    int tempRank, cnt;
    QPainter p(this);
    QMap<QString, ProgramInfo>::Iterator it;
    ProgramInfo *tempInfo;

    for (cnt = 0, it = programData.begin(); cnt < inList+inData; cnt++, ++it);

    tempInfo = &(it.data());

    tempRank = tempInfo->rank.toInt() + howMuch;
    if(tempRank > -100 && tempRank < 100) 
    {
        tempInfo->rank = QString::number(tempRank);
        if (sortType == byRank)
            SortList();
        updateList(&p);
        updateInfo(&p);
    }
}

void RankPrograms::saveRank(void) 
{
    QMap<QString, ProgramInfo>::Iterator it;
    ProgramInfo *tempInfo;
    QString *tempRank, key;

    for (it = programData.begin(); it != programData.end(); ++it) 
    {
        tempInfo = &(it.data());
        key = tempInfo->title + ":" + tempInfo->chanid + ":" + 
              tempInfo->startts.toString(Qt::ISODate);
        tempRank = &rankData[key];
        if (tempInfo->rank != *tempRank)
            tempInfo->ApplyRecordRankChange(db, tempInfo->rank);
    }
    ScheduledRecording::signalChange(db);
    gContext->SaveSetting("ProgramRankSorting", (int)sortType);
    done(MythDialog::Accepted);
}

void RankPrograms::FillList(void)
{
    QString key;
    int cnt = 999;

    programData.clear();

    vector<ProgramInfo *> recordinglist;
    QString cntStr = "";

    allowKeys = false;
    RemoteGetAllScheduledRecordings(recordinglist);
    allowKeys = true;

    vector<ProgramInfo *>::reverse_iterator pgiter = recordinglist.rbegin();

    for (; pgiter != recordinglist.rend(); pgiter++)
    {
        cntStr.sprintf("%d", cnt);
        programData[cntStr] = *(*pgiter);
        key = (*pgiter)->title + ":" + (*pgiter)->chanid + ":" + 
              (*pgiter)->startts.toString(Qt::ISODate);
        rankData[key] = (*pgiter)->rank;
        delete (*pgiter);
        cnt--;
        dataCount++;
    }
}

typedef struct RankInfo 
{
    ProgramInfo *prog;
    int cnt;
};

class titleSort 
{
    public:
        bool operator()(const RankInfo a, const RankInfo b) 
        {
            return (a.prog->title > b.prog->title);
        }
};

class programRankSort 
{
    public:
        bool operator()(const RankInfo a, const RankInfo b) 
        {
            if (a.prog->rank.toInt() == b.prog->rank.toInt())
                return (a.prog->title > b.prog->title);
            return (a.prog->rank.toInt() < b.prog->rank.toInt());
        }
};

void RankPrograms::SortList() 
{
    typedef vector<RankInfo> sortList;
    typedef QMap<QString, ProgramInfo> progMap;
    
    int i, j;
    bool cursorChanged = false;
    sortList sortedList;
    progMap::Iterator pit;
    sortList::iterator sit;
    ProgramInfo *progInfo;
    RankInfo *rankInfo;
    progMap sdCopy;
    QString cntStr = "";

    for (i = 0, pit = programData.begin(); pit != programData.end(); ++pit, i++)
    {
        progInfo = &(pit.data());
        RankInfo tmp = {progInfo, i};
        sortedList.push_back(tmp);
        sdCopy[pit.key()] = pit.data();
    }

    switch(sortType) 
    {
        case byTitle : sort(sortedList.begin(), sortedList.end(), titleSort());
                       break;
        case byRank : sort(sortedList.begin(), sortedList.end(), 
                           programRankSort());
                      break;
    }

    programData.clear();

    for (i = 0, sit = sortedList.begin(); sit != sortedList.end(); i++, ++sit)
    {
        rankInfo = &(*sit);

        for (j = 0, pit = sdCopy.begin(); j != rankInfo->cnt; j++, ++pit);

        progInfo = &(pit.data());
        cntStr.sprintf("%d", 999-i);
        programData[cntStr] = pit.data();

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

void RankPrograms::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    
    int pastSkip = (int)inData;
    pageDowner = false;
    listCount = 0;

    typedef QMap<QString, ProgramInfo> RankData;
    ProgramInfo *tempInfo;
  
    QString tempSubTitle = "";
    QString tempRank = "";

    RankData::Iterator it;
    RankData::Iterator start;
    RankData::Iterator end;

    start = programData.begin();
    end = programData.end();

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

                        tempSubTitle = tempInfo->title;
                        if ((tempInfo->subtitle).stripWhiteSpace().length() > 0)
                            tempSubTitle = tempSubTitle + " - \"" + 
                                           tempInfo->subtitle + "\"";

                        tempRank = tempInfo->rank;

                        if (cnt == inList)
                        {
                            if (curitem)
                                delete curitem;
                            curitem = new ProgramInfo(*tempInfo);
                            ltype->SetItemCurrent(cnt);
                        }

                        ltype->SetItemText(cnt, 1, tempSubTitle);
                        if(tempInfo->rank.toInt() >= 0)
                            ltype->SetItemText(cnt, 2, "+");
                        else if(tempInfo->rank.toInt() < 0)
                            ltype->SetItemText(cnt, 2, "-");
                        ltype->SetItemText(cnt, 3, 
                                        QString::number(abs(tempRank.toInt())));

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

    if (programData.count() <= 0)
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

void RankPrograms::updateInfo(QPainter *p)
{
    int rectyperank, chanrank, progrank, totalrank;
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    ScheduledRecording::RecordingType rectype; 

    if (programData.count() > 0 && curitem)
    {  
        progrank = curitem->rank.toInt();
        rectype = curitem->GetProgramRecordingStatus(db);
        rectyperank = curitem->getTypeRank(rectype);
        chanrank = curitem->getChannelRank(curitem->chanid, db);
        totalrank = progrank + rectyperank + chanrank;

        QString subtitle = "";
        if (curitem->subtitle != "(null)")
            subtitle = curitem->subtitle;
        else
            subtitle = "";

        LayerSet *container = NULL;
        container = theme->GetSet("program_info");
        if (container)
        {
            UITextType *type = (UITextType *)container->GetType("title");
            if (type)
                type->SetText(curitem->title);
 
            type = (UITextType *)container->GetType("subtitle");
            if (type)
                type->SetText(subtitle);

            type = (UITextType *)container->GetType("type");
            if (type) {
                QString text;
                switch (rectype)
                {
                    case ScheduledRecording::SingleRecord:
                        text = tr("Recording just this showing");
                        break;
                    case ScheduledRecording::WeekslotRecord:
                        text = tr("Recording every week");
                        break;
                    case ScheduledRecording::TimeslotRecord:
                        text = tr("Recording when shown in this timeslot");
                        break;
                    case ScheduledRecording::ChannelRecord:
                        text = tr("Recording when shown on this channel");
                        break;
                    case ScheduledRecording::AllRecord:
                        text = tr("Recording all showings");
                        break;
                    case ScheduledRecording::NotRecording:
                        text = tr("Not recording this showing");
                        break;
                    default:
                        text = tr("Error!");
                        break;
                }
                type->SetText(text);
            }

            type = (UITextType *)container->GetType("typerank");
            if (type) {
                type->SetText(QString::number(abs(rectyperank)));
            }
            type = (UITextType *)container->GetType("typesign");
            if (type) {
                if(rectyperank >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("channel");
            if (type) {
                if(curitem->channame != "")
                    type->SetText(curitem->channame);
                else
                    type->SetText("Any");
            }

            type = (UITextType *)container->GetType("channelrank");
            if (type) {
                type->SetText(QString::number(abs(chanrank)));
            }

            type = (UITextType *)container->GetType("channelsign");
            if (type) {
                if(chanrank >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("rank");
            if (type) {
                if(curitem->rank.toInt() >= 0)
                    type->SetText("+"+curitem->rank);
                else
                    type->SetText(curitem->rank);
            }

            type = (UITextType *)container->GetType("rankB");
            if (type) {
                type->SetText(QString::number(abs(progrank)));
            }

            type = (UITextType *)container->GetType("ranksign");
            if (type) {
                if(totalrank >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("totalrank");
            if (type) {
                if(totalrank >= 0)
                    type->SetText("+"+QString::number(totalrank));
                else
                    type->SetText(QString::number(totalrank));
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
