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

// overloaded version of ProgramInfo with additional rank
// values so we can keep everything together and don't
// have to hit the db mulitiple times
ProgramRankInfo::ProgramRankInfo(void) : ProgramInfo()
{
    channelRank = 0;
    recTypeRank = 0;
    recType = (ScheduledRecording::RecordingType)0;
}

ProgramRankInfo::ProgramRankInfo(const ProgramRankInfo &other) :
    ProgramInfo::ProgramInfo(other)
{
    channelRank = other.channelRank;
    recTypeRank = other.recTypeRank;
    recType = other.recType;
}

ProgramRankInfo& ProgramRankInfo::operator=(const ProgramInfo &other)
{
    title = other.title;
    subtitle = other.subtitle;
    description = other.description;
    category = other.category;
    chanid = other.chanid;
    chanstr = other.chanstr;
    chansign = other.chansign;
    channame = other.channame;
    pathname = other.pathname;
    filesize = other.filesize;
    hostname = other.hostname;

    startts = other.startts;
    endts = other.endts;
    spread = other.spread;
    startCol = other.startCol;

    conflicting = other.conflicting;
    recording = other.recording;
    duplicate = other.duplicate;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    schedulerid = other.schedulerid;
    rank = other.rank;

    return(*this);
}

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
    inList = inData = 0;
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
        case Key_Escape: saveRank();
                         gContext->SaveSetting("ProgramRankSorting", 
                                               (int)sortType);
                         done(MythDialog::Accepted);
                         break;
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
        case Key_I: saveRank(); 
                    edit(); 
                    break;
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

    ProgramRankInfo *rec = curitem;

    MythContext::KickDatabase(db);

    if (rec)
    {
        if ((gContext->GetNumSetting("AdvancedRecord", 0)) ||
            (rec->GetProgramRecordingStatus(db)
                > ScheduledRecording::AllRecord))
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

        // We need to refetch the rank values since the Advanced
        // Recording Options page could've been used to change them 
        QString thequery;
        int recid = rec->GetScheduledRecording(db)->getRecordID();
        thequery = QString("SELECT rank, type FROM record WHERE recordid = %1;")
                           .arg(recid);
        QSqlQuery query = db->exec(thequery);

        if (query.isActive())
            if (query.numRowsAffected() > 0)
            {
                query.next();
                QString rank = query.value(0).toString();
                int rectype = query.value(1).toInt();

                int cnt;
                QMap<QString, ProgramRankInfo>::Iterator it;
                ProgramRankInfo *progInfo;

                // iterate through programData till we hit the line where
                // the cursor currently is
                for (cnt = 0, it = programData.begin(); cnt < inList+inData; 
                     cnt++, ++it);
                progInfo = &(it.data());
           
                int rtRanks[5];
                rtRanks[0] = gContext->GetNumSetting("SingleRecordRank", 0);
                rtRanks[1] = gContext->GetNumSetting("TimeslotRecordRank", 0);
                rtRanks[2] = gContext->GetNumSetting("ChannelRecordRank", 0);
                rtRanks[3] = gContext->GetNumSetting("AllRecordRank", 0);
                rtRanks[4] = gContext->GetNumSetting("WeekslotRecordRank", 0);

                // set the ranks of that program 
                progInfo->rank = rank;
                progInfo->recType = (ScheduledRecording::RecordingType)rectype;
                progInfo->recTypeRank = rtRanks[progInfo->recType-1];

                // also set the origRankData with new rank so we don't
                // save to db again when we exit
                QString key = progInfo->title + ":" + progInfo->chanid + ":" + 
                              progInfo->startts.toString(Qt::ISODate);
                origRankData[key] = progInfo->rank;

                SortList();
            }
            else
            {
                // empty query means this recordid no longer exists
                // in record so it was deleted
                // remove it from programData
                int cnt;
                QMap<QString, ProgramRankInfo>::Iterator it;
                for (cnt = 0, it = programData.begin(); cnt < inList+inData; 
                     cnt++, ++it);
                programData.remove(it);
                SortList();
                inList = inData = 0;
            }
        else
            MythContext::DBError("Get new rank query", query);

        update(fullRect);
    }

    doingSel = false;
}

void RankPrograms::changeRank(int howMuch) 
{
    int tempRank, cnt;
    QPainter p(this);
    QMap<QString, ProgramRankInfo>::Iterator it;
    ProgramRankInfo *progInfo;
 
    // iterate through programData till we hit the line where
    // the cursor currently is
    for (cnt = 0, it = programData.begin(); cnt < inList+inData; cnt++, ++it);
    progInfo = &(it.data());

    // inc/dec rank
    tempRank = progInfo->rank.toInt() + howMuch;
    if(tempRank > -100 && tempRank < 100) 
    {
        progInfo->rank = QString::number(tempRank);

        // order may change if sorting by rank, so resort
        if (sortType == byRank)
            SortList();
        updateList(&p);
        updateInfo(&p);
    }
}

void RankPrograms::saveRank(void) 
{
    QMap<QString, ProgramRankInfo>::Iterator it;

    for (it = programData.begin(); it != programData.end(); ++it) 
    {
        ProgramRankInfo *progInfo = &(it.data());
        QString key = progInfo->title + ":" + progInfo->chanid + ":" + 
                      progInfo->startts.toString(Qt::ISODate);

        // if this program's rank changed from when we entered
        // save new value out to db
        if (progInfo->rank != origRankData[key])
            progInfo->ApplyRecordRankChange(db, progInfo->rank);
    }
    ScheduledRecording::signalChange(db);
}

void RankPrograms::FillList(void)
{
    int cnt = 999, rtRanks[5];
    vector<ProgramInfo *> recordinglist;

    programData.clear();

    allowKeys = false;
    RemoteGetAllScheduledRecordings(recordinglist);
    allowKeys = true;

    vector<ProgramInfo *>::reverse_iterator pgiter = recordinglist.rbegin();

    for (; pgiter != recordinglist.rend(); pgiter++)
    {
        programData[QString::number(cnt)] = *(*pgiter);

        // save rank value in map so we don't have to save all program's
        // rank values when we exit
        QString key = (*pgiter)->title + ":" + (*pgiter)->chanid + ":" + 
                      (*pgiter)->startts.toString(Qt::ISODate);
        origRankData[key] = (*pgiter)->rank;

        delete (*pgiter);
        cnt--;
        dataCount++;
    }

//    cerr << "RemoteGetAllScheduledRecordings() returned " << programData.size();
//    cerr << " programs" << endl;

    // get all the recording type rank values
    rtRanks[0] = gContext->GetNumSetting("SingleRecordRank", 0);
    rtRanks[1] = gContext->GetNumSetting("TimeslotRecordRank", 0);
    rtRanks[2] = gContext->GetNumSetting("ChannelRecordRank", 0);
    rtRanks[3] = gContext->GetNumSetting("AllRecordRank", 0);
    rtRanks[4] = gContext->GetNumSetting("WeekslotRecordRank", 0);
    
    // get channel ranks and recording types associated with each
    // program from db
    // (hope this is ok to do here, it's so much lighter doing
    // it all at once than once per program)
    QString query = QString("SELECT record.title, record.chanid, "
                            "record.starttime, record.startdate, "
                            "record.type, channel.rank "
                            "FROM record "
                            "LEFT JOIN channel ON "
                            "(record.chanid = channel.chanid);");

    QSqlQuery result = db->exec(query);
   
//    cerr << "db query returned " << result.numRowsAffected() << " programs";
//    cerr << endl; 
    int matches = 0;

    if (result.isActive() && result.numRowsAffected() > 0)
    {
        while (result.next()) 
        {
            QString title = QString::fromUtf8(result.value(0).toString());
            QString chanid = result.value(1).toString();
            QString tempTime = result.value(2).toString();
            QString tempDate = result.value(3).toString();
            ScheduledRecording::RecordingType recType = 
               (ScheduledRecording::RecordingType)result.value(4).toInt();
            int channelRank = result.value(5).toInt();
            int recTypeRank = rtRanks[recType-1];
              
            // this is so kludgy
            // since we key off of title+chanid+startts we have
            // to copy what RemoteGetAllScheduledRecordings()
            // does so the keys will match 
            QDateTime startts; 
            if (recType == ScheduledRecording::SingleRecord ||
                recType == ScheduledRecording::TimeslotRecord ||
                recType == ScheduledRecording::WeekslotRecord)
                startts = QDateTime::fromString(tempDate+":"+tempTime,
                                                Qt::ISODate);
            else
            {
                startts = QDateTime::currentDateTime();
                startts.setTime(QTime(0,0));
            }
    
            // make a key for each program
            QString keyA = title + ":" + chanid + ":" +
                           startts.toString(Qt::ISODate);

            // find matching program in programData and set
            // channelRank, recTypeRank and recType
            QMap<QString, ProgramRankInfo>::Iterator it;
            for (it = programData.begin(); it != programData.end(); ++it)
            {
                ProgramRankInfo *progInfo = &(it.data());
                QString keyB = progInfo->title + ":" + progInfo->chanid + ":" +
                               progInfo->startts.toString(Qt::ISODate);
                if(keyA == keyB)
                {
                    progInfo->channelRank = channelRank;
                    progInfo->recTypeRank = recTypeRank;
                    progInfo->recType = recType;
                    matches++;
                    break;
                }
            }
        }
    }
    else
        MythContext::DBError("Get program ranks query", query);

//    cerr << matches << " matches made" << endl;
}

typedef struct RankInfo 
{
    ProgramRankInfo *prog;
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
            int finalA = a.prog->rank.toInt() + a.prog->channelRank +
                         a.prog->recTypeRank;
            int finalB = b.prog->rank.toInt() + b.prog->channelRank +
                         b.prog->recTypeRank;

            if (finalA == finalB)
                return (a.prog->title > b.prog->title);
            return (finalA < finalB);
        }
};

void RankPrograms::SortList() 
{
    int i, j;
    bool cursorChanged = false;
    vector<RankInfo> sortedList;
    QMap<QString, ProgramRankInfo>::Iterator pit;
    vector<RankInfo>::iterator sit;
    ProgramRankInfo *progInfo;
    RankInfo *rankInfo;
    QMap<QString, ProgramRankInfo> pdCopy;

    // copy programData into sortedList and make a copy
    // of programData in pdCopy
    for (i = 0, pit = programData.begin(); pit != programData.end(); ++pit, i++)
    {
        progInfo = &(pit.data());
        RankInfo tmp = {progInfo, i};
        sortedList.push_back(tmp);
        pdCopy[pit.key()] = pit.data();
    }

    // sort sortedList
    switch(sortType) 
    {
        case byTitle : sort(sortedList.begin(), sortedList.end(), titleSort());
                       break;
        case byRank : sort(sortedList.begin(), sortedList.end(), 
                           programRankSort());
                      break;
    }

    programData.clear();

    // rebuild programData in sortedList order from pdCopy
    for (i = 0, sit = sortedList.begin(); sit != sortedList.end(); i++, ++sit)
    {
        rankInfo = &(*sit);

        // find rankInfo[i] in pdCopy 
        for (j = 0, pit = pdCopy.begin(); j != rankInfo->cnt; j++, ++pit);

        progInfo = &(pit.data());

        // put back into programData
        programData[QString::number(999-i)] = pit.data();

        // if rankInfo[i] is the program where the cursor
        // was pre-sort then we need to update to cursor
        // to the ith position
        if (!cursorChanged && rankInfo->cnt == inList+inData) 
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

void RankPrograms::updateList(QPainter *p)
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
        UIListType *ltype = (UIListType *)container->GetType("ranklist");
        if (ltype)
        {
            int cnt = 0;
            ltype->ResetList();
            ltype->SetActive(true);

            QMap<QString, ProgramRankInfo>::Iterator it;
            for (it = programData.begin(); it != programData.end(); ++it)
            {
                if (cnt < listsize)
                {
                    if (pastSkip <= 0)
                    {
                        ProgramRankInfo *progInfo = &(it.data());

                        QString key = progInfo->title + ":" + 
                                      progInfo->chanid + ":" +
                                      progInfo->startts.toString(Qt::ISODate);

                        int progRank = progInfo->rank.toInt();
                        int finalRank = progRank + progInfo->channelRank +
                                        progInfo->recTypeRank;
        
                        QString tempSubTitle = progInfo->title;
                        if ((progInfo->subtitle).stripWhiteSpace().length() > 0)
                            tempSubTitle = tempSubTitle + " - \"" + 
                                           progInfo->subtitle + "\"";

                        if (cnt == inList)
                        {
                            if (curitem)
                                delete curitem;
                            curitem = new ProgramRankInfo(*progInfo);
                            ltype->SetItemCurrent(cnt);
                        }

                        ltype->SetItemText(cnt, 1, tempSubTitle);

                        if (progRank >= 0)
                            ltype->SetItemText(cnt, 2, "+");
                        else if (progRank < 0)
                            ltype->SetItemText(cnt, 2, "-");
                        ltype->SetItemText(cnt, 3, 
                                        QString::number(abs(progRank)));

                        if (finalRank >= 0)
                            ltype->SetItemText(cnt, 4, "+");
                        else if (finalRank < 0)
                            ltype->SetItemText(cnt, 4, "-");

                        ltype->SetItemText(cnt, 5, 
                                           QString::number(abs(finalRank)));

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
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (programData.count() > 0 && curitem)
    {  
        int progRank, chanrank, rectyperank, finalRank;
        ScheduledRecording::RecordingType rectype; 

        progRank = curitem->rank.toInt();
        chanrank = curitem->channelRank;
        rectyperank = curitem->recTypeRank;
        finalRank = progRank + chanrank + rectyperank;

        rectype = curitem->recType;

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
                type->SetText(QString::number(abs(progRank)));
            }

            type = (UITextType *)container->GetType("ranksign");
            if (type) {
                if (finalRank >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("finalrank");
            if (type) {
                if (finalRank >= 0)
                    type->SetText("+"+QString::number(finalRank));
                else
                    type->SetText(QString::number(finalRank));
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
