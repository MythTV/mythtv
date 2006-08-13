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

#include "programrecpriority.h"
#include "scheduledrecording.h"
#include "customedit.h"
#include "proglist.h"
#include "tv.h"

#include "exitcodes.h"
#include "dialogbox.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "remoteutil.h"

// overloaded version of ProgramInfo with additional recording priority
// values so we can keep everything together and don't
// have to hit the db mulitiple times
ProgramRecPriorityInfo::ProgramRecPriorityInfo(void) : ProgramInfo()
{
    recTypeRecPriority = 0;
    recType = kNotRecording;
}

ProgramRecPriorityInfo::ProgramRecPriorityInfo(const ProgramRecPriorityInfo &other) 
                      : ProgramInfo::ProgramInfo(other)
{
    recTypeRecPriority = other.recTypeRecPriority;
    recType = other.recType;
}

ProgramRecPriorityInfo& ProgramRecPriorityInfo::operator=(const ProgramInfo &other)
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

    recstatus = other.recstatus;
    recordid = other.recordid;
    rectype = other.rectype;
    dupin = other.dupin;
    dupmethod = other.dupmethod;
    recgroup = other.recgroup;
    playgroup = other.playgroup;
    chancommfree = other.chancommfree;

    sourceid = other.sourceid;
    inputid = other.inputid;
    cardid = other.cardid;
    schedulerid = other.schedulerid;
    recpriority = other.recpriority;

    seriesid = other.seriesid;
    programid = other.programid;

    return(*this);
}

ProgramRecPriority::ProgramRecPriority(MythMainWindow *parent, 
                             const char *name)
            : MythDialog(parent, name)
{
    curitem = NULL;
    bgTransBackup = NULL;
    pageDowner = false;

    listCount = 0;
    dataCount = 0;

    fullRect = QRect(0, 0, size().width(), size().height());
    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, "recpriorityprograms"))
    {
        DialogBox diag(gContext->GetMainWindow(), tr("The theme you are using "
                       "does not contain a 'recpriorityprograms' element.  "
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
        VERBOSE(VB_IMPORTANT, "MythFrontEnd::ProgramRecPriority(): "
                "Failed to get selector object.");
        exit(FRONTEND_BUGGY_EXIT_NO_SELECTOR);
    }

    bgTransBackup = gContext->LoadScalePixmap("trans-backup.png");
    if (!bgTransBackup)
        bgTransBackup = new QPixmap();

    updateBackground();

    FillList();
    sortType = (SortType)gContext->GetNumSetting("ProgramRecPrioritySorting", 
                                                 (int)byTitle);
    reverseSort = gContext->GetNumSetting("ProgramRecPriorityReverse", 0);

    SortList(); 
    inList = inData = 0;
    setNoErase();

    gContext->addListener(this);
    gContext->addCurrentLocation("ProgramRecPriority");
}

ProgramRecPriority::~ProgramRecPriority()
{
    gContext->removeListener(this);
    gContext->removeCurrentLocation();
    delete theme;
    if (bgTransBackup)
        delete bgTransBackup;
    if (curitem)
        delete curitem;
}

void ProgramRecPriority::keyPressEvent(QKeyEvent *e)
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
            else if (action == "RANKINC")
                changeRecPriority(1);
            else if (action == "RANKDEC")
                changeRecPriority(-1);
            else if ((action == "PAUSE") || (action == "PLAYBACK"))
                deactivate();
            else if (action == "ESCAPE" || action == "LEFT")
            {
                saveRecPriority();
                gContext->SaveSetting("ProgramRecPrioritySorting",
                                      (int)sortType);
                gContext->SaveSetting("ProgramRecPriorityReverse",
                                      (int)reverseSort);
                done(MythDialog::Accepted);
            }
            else if (action == "1")
            {
                if (sortType != byTitle)
                {
                    sortType = byTitle;
                    reverseSort = false;
                }
                else
                {
                    reverseSort = !reverseSort;
                }
                SortList();
                update(fullRect);
            }
            else if (action == "2")
            {
                if (sortType != byRecPriority)
                {
                    sortType = byRecPriority;
                    reverseSort = false;
                }
                else
                {
                    reverseSort = !reverseSort;
                }
                SortList();
                update(fullRect);
            }
            else if (action == "4")
            {
                if (sortType != byRecType)
                {
                    sortType = byRecType;
                    reverseSort = false;
                }
                else
                {
                    reverseSort = !reverseSort;
                }
                SortList();
                update(fullRect);
            }
            else if (action == "5")
            {
                if (sortType != byCount)
                {
                    sortType = byCount;
                    reverseSort = false;
                }
                else
                {
                    reverseSort = !reverseSort;
                }
                SortList();
                update(fullRect);
            }
            else if (action == "6")
            {
                if (sortType != byRecCount)
                {
                    sortType = byRecCount;
                    reverseSort = false;
                }
                else
                {
                    reverseSort = !reverseSort;
                }
                SortList();
                update(fullRect);
            }
            else if (action == "PREVVIEW" || action == "NEXTVIEW")
            {
                reverseSort = false;
                if (sortType == byTitle)
                    sortType = byRecPriority;
                else if (sortType == byRecPriority)
                    sortType = byRecType;
                else
                    sortType = byTitle;
                SortList();
                update(fullRect);
            }
            else if (action == "SELECT" || action == "MENU" ||
                     action == "INFO")
            {
                saveRecPriority();
                edit();
            }
            else if (action == "CUSTOMEDIT")
            {
                saveRecPriority();
                customEdit();
            }
            else if (action == "UPCOMING")
            {
                saveRecPriority();
                upcoming();
            }
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void ProgramRecPriority::LoadWindow(QDomElement &element)
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
                VERBOSE(VB_IMPORTANT,
                        QString("ProgramRecPriority: Unknown child element: "
                                "%1. Ignoring.").arg(e.tagName()));
            }
        }
    }
}

void ProgramRecPriority::parseContainer(QDomElement &element)
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

void ProgramRecPriority::updateBackground(void)
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

void ProgramRecPriority::paintEvent(QPaintEvent *e)
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

void ProgramRecPriority::cursorDown(bool page)
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

void ProgramRecPriority::cursorUp(bool page)
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

void ProgramRecPriority::edit(void)
{
    if (!curitem)
        return;

    ProgramRecPriorityInfo *rec = curitem;

    if (rec)
    {
        int recid = 0;
        ScheduledRecording record;
        record.loadByID(rec->recordid);
        if (record.getSearchType() == kNoSearch)
            record.loadByProgram(rec);
        record.exec();
        recid = record.getRecordID();

        // We need to refetch the recording priority values since the Advanced
        // Recording Options page could've been used to change them 

        if (!recid)
            recid = rec->getRecordID();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT recpriority, type, inactive FROM"
                      " record WHERE recordid = :RECORDID ;");
        query.bindValue(":RECORDID", recid);

        if (query.exec() && query.isActive())
            if (query.size() > 0)
            {
                query.next();
                int recPriority = query.value(0).toInt();
                int rectype = query.value(1).toInt();
                int inactive = query.value(2).toInt();

                int cnt;
                QMap<QString, ProgramRecPriorityInfo>::Iterator it;
                ProgramRecPriorityInfo *progInfo;

                // iterate through programData till we hit the line where
                // the cursor currently is
                for (cnt = 0, it = programData.begin(); cnt < inList+inData; 
                     cnt++, ++it);
                progInfo = &(it.data());
           
                int rtRecPriors[11];
                rtRecPriors[0] = 0;
                rtRecPriors[kSingleRecord] = 
                    gContext->GetNumSetting("SingleRecordRecPriority", 1);
                rtRecPriors[kTimeslotRecord] = 
                    gContext->GetNumSetting("TimeslotRecordRecPriority", 0);
                rtRecPriors[kChannelRecord] = 
                    gContext->GetNumSetting("ChannelRecordRecPriority", 0);
                rtRecPriors[kAllRecord] = 
                    gContext->GetNumSetting("AllRecordRecPriority", 0);
                rtRecPriors[kWeekslotRecord] = 
                    gContext->GetNumSetting("WeekslotRecordRecPriority", 0);
                rtRecPriors[kFindOneRecord] = 
                    gContext->GetNumSetting("FindOneRecordRecPriority", -1);
                rtRecPriors[kOverrideRecord] = 
                    gContext->GetNumSetting("OverrideRecordRecPriority", 0);
                rtRecPriors[kDontRecord] = 
                    gContext->GetNumSetting("OverrideRecordRecPriority", 0);
                rtRecPriors[kFindDailyRecord] = 
                    gContext->GetNumSetting("FindOneRecordRecPriority", -1);
                rtRecPriors[kFindWeeklyRecord] = 
                    gContext->GetNumSetting("FindOneRecordRecPriority", -1);

                // set the recording priorities of that program 
                progInfo->recpriority = recPriority;
                progInfo->recType = (RecordingType)rectype;
                progInfo->recTypeRecPriority = rtRecPriors[progInfo->recType];
                // also set the origRecPriorityData with new recording 
                // priority so we don't save to db again when we exit
                origRecPriorityData[progInfo->recordid] = 
                                                        progInfo->recpriority;
                // also set the active/inactive state
                progInfo->recstatus = inactive ? rsInactive : rsWillRecord;

                SortList();
            }
            else
            {
                // empty query means this recordid no longer exists
                // in record so it was deleted
                // remove it from programData
                int cnt;
                QMap<QString, ProgramRecPriorityInfo>::Iterator it;
                for (cnt = 0, it = programData.begin(); cnt < inList+inData; 
                     cnt++, ++it);
                programData.remove(it);
                SortList();
                delete curitem;
                curitem = NULL;
                dataCount--;

                if (cnt >= dataCount)
                    cnt = dataCount - 1;
                if (dataCount <= listsize || cnt <= listsize / 2)
                    inData = 0;
                else if (cnt >= dataCount - listsize + listsize / 2)
                    inData = dataCount - listsize;
                else
                    inData = cnt - listsize / 2;
                inList = cnt - inData;
            }
        else
            MythContext::DBError("Get new recording priority query", query);

        countMatches();
        update(fullRect);
    }
}

void ProgramRecPriority::customEdit(void)
{
    if (!curitem)
        return;

    ScheduledRecording record;
    record.loadByID(curitem->recordid);

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(),
                                        "customedit", curitem);
    ce->exec();
    delete ce;
}

void ProgramRecPriority::deactivate(void)
{
    if (!curitem)
        return;

    ProgramRecPriorityInfo *rec = curitem;

    if (rec)
    {
        MSqlQuery query(MSqlQuery::InitCon());

        query.prepare("SELECT inactive FROM record "
                      "WHERE recordid = :RECID ;");
        query.bindValue(":RECID", rec->recordid);

        int inactive = 0;
        if (query.exec() && query.isActive())
            if (query.size() > 0)
            {
                query.next();
                inactive = query.value(0).toInt();
                if (inactive)
                    inactive = 0;
                else
                    inactive = 1;

                query.prepare("UPDATE record SET inactive = :INACTIVE "
                              "WHERE recordid = :RECID ;");
                query.bindValue(":INACTIVE", inactive);
                query.bindValue(":RECID", rec->recordid);

                if (query.exec() && query.isActive())
                {
                    ScheduledRecording::signalChange(0);
                    int cnt;
                    QMap<QString, ProgramRecPriorityInfo>::Iterator it;
                    ProgramRecPriorityInfo *progInfo;

                    // iterate through programData till we hit the line where
                    // the cursor currently is
                    for (cnt = 0, it = programData.begin(); cnt < inList+inData;
                         cnt++, ++it);
                    progInfo = &(it.data());
                    progInfo->recstatus = inactive ? rsInactive : rsWillRecord;
                } else
                    MythContext::DBError("Update recording schedule inactive query", query);
            }

        QPainter p(this);
        updateInfo(&p);
        update(fullRect);
    }
}

void ProgramRecPriority::upcoming(void)
{
    if (!curitem)
        return;

    ScheduledRecording record;

    record.loadByID(curitem->recordid);
    record.runRuleList();
}

void ProgramRecPriority::changeRecPriority(int howMuch) 
{
    int tempRecPriority, cnt;
    QPainter p(this);
    QMap<QString, ProgramRecPriorityInfo>::Iterator it;
    ProgramRecPriorityInfo *progInfo;
 
    // iterate through programData till we hit the line where
    // the cursor currently is
    for (cnt = 0, it = programData.begin(); cnt < inList+inData; cnt++, ++it);
    progInfo = &(it.data());

    // inc/dec recording priority
    tempRecPriority = progInfo->recpriority + howMuch;
    if (tempRecPriority > -100 && tempRecPriority < 100) 
    {
        progInfo->recpriority = tempRecPriority;

        // order may change if sorting by recording priority, so resort
        if (sortType == byRecPriority)
            SortList();
        updateList(&p);
        updateInfo(&p);
    }
}

void ProgramRecPriority::saveRecPriority(void) 
{
    QMap<QString, ProgramRecPriorityInfo>::Iterator it;

    for (it = programData.begin(); it != programData.end(); ++it) 
    {
        ProgramRecPriorityInfo *progInfo = &(it.data());
        int key = progInfo->recordid; 

        // if this program's recording priority changed from when we entered
        // save new value out to db
        if (progInfo->recpriority != origRecPriorityData[key])
            progInfo->ApplyRecordRecPriorityChange(progInfo->recpriority);
    }
}

void ProgramRecPriority::FillList(void)
{
    int cnt = 999, rtRecPriors[11];
    vector<ProgramInfo *> recordinglist;

    programData.clear();

    RemoteGetAllScheduledRecordings(recordinglist);

    vector<ProgramInfo *>::reverse_iterator pgiter = recordinglist.rbegin();

    for (; pgiter != recordinglist.rend(); pgiter++)
    {
        programData[QString::number(cnt)] = *(*pgiter);

        // save recording priority value in map so we don't have to 
        // save all program's recording priority values when we exit
        origRecPriorityData[(*pgiter)->recordid] = (*pgiter)->recpriority;

        delete (*pgiter);
        cnt--;
        dataCount++;
    }

//    cerr << "RemoteGetAllScheduledRecordings() returned " << programData.size();
//    cerr << " programs" << endl;

    // get all the recording type recording priority values
    rtRecPriors[0] = 0;
    rtRecPriors[kSingleRecord] = 
        gContext->GetNumSetting("SingleRecordRecPriority", 1);
    rtRecPriors[kTimeslotRecord] = 
        gContext->GetNumSetting("TimeslotRecordRecPriority", 0);
    rtRecPriors[kChannelRecord] = 
        gContext->GetNumSetting("ChannelRecordRecPriority", 0);
    rtRecPriors[kAllRecord] = 
        gContext->GetNumSetting("AllRecordRecPriority", 0);
    rtRecPriors[kWeekslotRecord] = 
        gContext->GetNumSetting("WeekslotRecordRecPriority", 0);
    rtRecPriors[kFindOneRecord] = 
        gContext->GetNumSetting("FindOneRecordRecPriority", -1);
    rtRecPriors[kOverrideRecord] = 
        gContext->GetNumSetting("OverrideRecordRecPriority", 0);
    rtRecPriors[kDontRecord] = 
        gContext->GetNumSetting("OverrideRecordRecPriority", 0);
    rtRecPriors[kFindDailyRecord] = 
        gContext->GetNumSetting("FindOneRecordRecPriority", -1);
    rtRecPriors[kFindWeeklyRecord] = 
        gContext->GetNumSetting("FindOneRecordRecPriority", -1);
    
    // get recording types associated with each program from db
    // (hope this is ok to do here, it's so much lighter doing
    // it all at once than once per program)

    MSqlQuery result(MSqlQuery::InitCon());
    result.prepare("SELECT recordid, record.title, record.chanid, "
                   "record.starttime, record.startdate, "
                   "record.type, record.inactive "
                   "FROM record;");
   
    if (result.exec() && result.isActive() && result.size() > 0)
    {
        countMatches();

        while (result.next()) 
        {
            int recordid = result.value(0).toInt();
            QString title = QString::fromUtf8(result.value(1).toString());
            QString chanid = result.value(2).toString();
            QString tempTime = result.value(3).toString();
            QString tempDate = result.value(4).toString();
            RecordingType recType = (RecordingType)result.value(5).toInt();
            int recTypeRecPriority = rtRecPriors[recType];
            int inactive = result.value(6).toInt();

            // find matching program in programData and set
            // recTypeRecPriority and recType
            QMap<QString, ProgramRecPriorityInfo>::Iterator it;
            for (it = programData.begin(); it != programData.end(); ++it)
            {
                ProgramRecPriorityInfo *progInfo = &(it.data());

                if (progInfo->recordid == recordid)
                {
                    progInfo->sortTitle = progInfo->title;
                    progInfo->sortTitle.remove(QRegExp(tr("^(The |A |An )")));

                    progInfo->recTypeRecPriority = recTypeRecPriority;
                    progInfo->recType = recType;
                    progInfo->recstatus = inactive ? rsInactive : rsWillRecord;
                    progInfo->matchCount = listMatch[progInfo->recordid];
                    progInfo->recCount = recMatch[progInfo->recordid];
                    break;
                }
            }
        }
    }
    else
        MythContext::DBError("Get program recording priorities query", result);
}

void ProgramRecPriority::countMatches()
{
    listMatch.clear();
    conMatch.clear();
    nowMatch.clear();
    recMatch.clear();
    ProgramList schedList;
    schedList.FromScheduler();
    QDateTime now = QDateTime::currentDateTime();

    ProgramInfo *s;
    for (s = schedList.first(); s; s = schedList.next())
    {
        if (s->recendts > now && s->recstatus != rsNotListed)
        {
            listMatch[s->recordid]++;
            if (s->recstatus == rsConflict ||
                s->recstatus == rsOffLine)
                conMatch[s->recordid]++;
            else if (s->recstatus == rsWillRecord)
                recMatch[s->recordid]++;
            else if (s->recstatus == rsRecording)
            {
                nowMatch[s->recordid]++;
                recMatch[s->recordid]++;
            }
        }
    }
}

typedef struct RecPriorityInfo 
{
    ProgramRecPriorityInfo *prog;
    int cnt;
};

class titleSort 
{
    public:
        titleSort(bool reverseSort = false) {m_reverse = reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            if (a.prog->sortTitle != b.prog->sortTitle)
            {
                if (m_reverse)
                    return (a.prog->sortTitle < b.prog->sortTitle);
                else
                    return (a.prog->sortTitle > b.prog->sortTitle);
            }

            int finalA = a.prog->recpriority + a.prog->recTypeRecPriority;
            int finalB = b.prog->recpriority + b.prog->recTypeRecPriority;
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return typeA < typeB;
                else
                    return typeA > typeB;
            }

            if (m_reverse)
                return a.prog->recordid < b.prog->recordid;
            else
                return a.prog->recordid > b.prog->recordid;
        }

    private:
        bool m_reverse;
};

class programRecPrioritySort 
{
    public:
        programRecPrioritySort(bool reverseSort = false)
                               {m_reverse = reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            int finalA = a.prog->recpriority + a.prog->recTypeRecPriority;
            int finalB = b.prog->recpriority + b.prog->recTypeRecPriority;
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return typeA < typeB;
                else
                    return typeA > typeB;
            }

            if (m_reverse)
                return a.prog->recordid < b.prog->recordid;
            else
                return a.prog->recordid > b.prog->recordid;
        }

    private:
        bool m_reverse;
};

class programRecTypeSort 
{
    public:
        programRecTypeSort(bool reverseSort = false)
                               {m_reverse = reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            int typeA = RecTypePriority(a.prog->recType);
            int typeB = RecTypePriority(b.prog->recType);
            if (typeA != typeB)
            {
                if (m_reverse)
                    return (typeA < typeB);
                else
                    return (typeA > typeB);
            }

            int finalA = a.prog->recpriority + a.prog->recTypeRecPriority;
            int finalB = b.prog->recpriority + b.prog->recTypeRecPriority;
            if (finalA != finalB)
            {
                if (m_reverse)
                    return finalA > finalB;
                else
                    return finalA < finalB;
            }

            if (m_reverse)
                return a.prog->recordid < b.prog->recordid;
            else
                return a.prog->recordid > b.prog->recordid;
        }

    private:
        bool m_reverse;
};

class programCountSort 
{
    public:
        programCountSort(bool reverseSort = false) {m_reverse = reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            int countA = a.prog->matchCount;
            int countB = b.prog->matchCount;
            int recCountA = a.prog->recCount;
            int recCountB = b.prog->recCount;

            if (countA != countB)
            {
                if (m_reverse)
                    return countA > countB;
                else
                    return countA < countB;
            }
            if (recCountA != recCountB)
            {
                if (m_reverse)
                    return recCountA > recCountB;
                else
                    return recCountA < recCountB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

class programRecCountSort 
{
    public:
        programRecCountSort(bool reverseSort=false) {m_reverse = reverseSort;}

        bool operator()(const RecPriorityInfo a, const RecPriorityInfo b) 
        {
            int countA = a.prog->matchCount;
            int countB = b.prog->matchCount;
            int recCountA = a.prog->recCount;
            int recCountB = b.prog->recCount;

            if (recCountA != recCountB)
            {
                if (m_reverse)
                    return recCountA > recCountB;
                else
                    return recCountA < recCountB;
            }
            if (countA != countB)
            {
                if (m_reverse)
                    return countA > countB;
                else
                    return countA < countB;
            }
            return (a.prog->sortTitle > b.prog->sortTitle);
        }

    private:
        bool m_reverse;
};

void ProgramRecPriority::SortList() 
{
    int i, j;
    bool cursorChanged = false;
    vector<RecPriorityInfo> sortedList;
    QMap<QString, ProgramRecPriorityInfo>::Iterator pit;
    vector<RecPriorityInfo>::iterator sit;
    ProgramRecPriorityInfo *progInfo;
    RecPriorityInfo *recPriorityInfo;
    QMap<QString, ProgramRecPriorityInfo> pdCopy;

    // copy programData into sortedList and make a copy
    // of programData in pdCopy
    for (i = 0, pit = programData.begin(); pit != programData.end(); ++pit, i++)
    {
        progInfo = &(pit.data());
        RecPriorityInfo tmp = {progInfo, i};
        sortedList.push_back(tmp);
        pdCopy[pit.key()] = pit.data();
    }

    // sort sortedList
    switch(sortType) 
    {
        case byTitle :
                 if (reverseSort)
                     sort(sortedList.begin(), sortedList.end(),
                          titleSort(true));
                 else
                     sort(sortedList.begin(), sortedList.end(), titleSort());
                 break;
        case byRecPriority :
                 if (reverseSort)
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecPrioritySort(true));
                 else
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecPrioritySort());
                 break;
        case byRecType :
                 if (reverseSort)
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecTypeSort(true));
                 else
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecTypeSort());
                 break;
        case byCount :
                 if (reverseSort)
                     sort(sortedList.begin(), sortedList.end(), 
                          programCountSort(true));
                 else
                     sort(sortedList.begin(), sortedList.end(), 
                          programCountSort());
                 break;
        case byRecCount :
                 if (reverseSort)
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecCountSort(true));
                 else
                     sort(sortedList.begin(), sortedList.end(), 
                          programRecCountSort());
                 break;
    }

    programData.clear();

    // rebuild programData in sortedList order from pdCopy
    for (i = 0, sit = sortedList.begin(); sit != sortedList.end(); i++, ++sit)
    {
        recPriorityInfo = &(*sit);

        // find recPriorityInfo[i] in pdCopy 
        for (j = 0,pit = pdCopy.begin(); j != recPriorityInfo->cnt; j++, ++pit);

        progInfo = &(pit.data());

        // put back into programData
        programData[QString::number(999-i)] = pit.data();

        // if recPriorityInfo[i] is the program where the cursor
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

void ProgramRecPriority::updateList(QPainter *p)
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

            QMap<QString, ProgramRecPriorityInfo>::Iterator it;
            for (it = programData.begin(); it != programData.end(); ++it)
            {
                if (cnt < listsize)
                {
                    if (pastSkip <= 0)
                    {
                        ProgramRecPriorityInfo *progInfo = &(it.data());

                        int progRecPriority = progInfo->recpriority;
                        int finalRecPriority = progRecPriority + 
                                        progInfo->recTypeRecPriority;
        
                        QString tempSubTitle = progInfo->title;
                        if ((progInfo->rectype == kSingleRecord ||
                             progInfo->rectype == kOverrideRecord ||
                             progInfo->rectype == kDontRecord) &&
                            (progInfo->subtitle).stripWhiteSpace().length() > 0)
                            tempSubTitle = tempSubTitle + " - \"" + 
                                           progInfo->subtitle + "\"";

                        if (cnt == inList)
                        {
                            if (curitem)
                                delete curitem;
                            curitem = new ProgramRecPriorityInfo(*progInfo);
                            ltype->SetItemCurrent(cnt);
                        }

                        ltype->SetItemText(cnt, 1, progInfo->RecTypeChar());
                        ltype->SetItemText(cnt, 2, tempSubTitle);

                        if (progRecPriority < 0)
                            ltype->SetItemText(cnt, 3, "-");
                        else
                            ltype->SetItemText(cnt, 3, "+");
                        ltype->SetItemText(cnt, 4, 
                                QString::number(abs(progRecPriority)));

                        if (finalRecPriority < 0)
                            ltype->SetItemText(cnt, 5, "-");
                        else
                            ltype->SetItemText(cnt, 5, "+");

                        ltype->SetItemText(cnt, 6, 
                                QString::number(abs(finalRecPriority)));

                        if (progInfo->recType == kDontRecord ||
                            progInfo->recstatus == rsInactive)
                            ltype->EnableForcedFont(cnt, "inactive");
                        else if (conMatch[progInfo->recordid] > 0)
                            ltype->EnableForcedFont(cnt, "conflicting");
                        else if (nowMatch[progInfo->recordid] > 0)
                            ltype->EnableForcedFont(cnt, "recording");
                        else if (recMatch[progInfo->recordid] > 0)
                            ltype->EnableForcedFont(cnt, "record");

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

void ProgramRecPriority::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    if (programData.count() > 0 && curitem)
    {  
        int progRecPriority, rectyperecpriority, finalRecPriority;
        RecordingType rectype; 

        progRecPriority = curitem->recpriority;
        rectyperecpriority = curitem->recTypeRecPriority;
        finalRecPriority = progRecPriority + rectyperecpriority;

        rectype = curitem->recType;

        QString subtitle = "";
        if (curitem->subtitle != "(null)" &&
            (curitem->rectype == kSingleRecord ||
             curitem->rectype == kOverrideRecord ||
             curitem->rectype == kDontRecord))
        {
            subtitle = curitem->subtitle;
        }

        QString matchInfo;
        if (curitem->recstatus == rsInactive)
            matchInfo = QString("%1 %2").arg(listMatch[curitem->recordid])
                                        .arg(curitem->RecStatusText());
        else
            matchInfo = QString(tr("Recording %1 of %2"))
                                   .arg(recMatch[curitem->recordid])
                                   .arg(listMatch[curitem->recordid]);

        subtitle = QString("(%1) %2").arg(matchInfo).arg(subtitle);

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
                    case kSingleRecord:
                        text = tr("Recording just this showing");
                        break;
                    case kOverrideRecord:
                        text = tr("Recording with override options");
                        break;
                    case kWeekslotRecord:
                        text = tr("Recording every week");
                        break;
                    case kTimeslotRecord:
                        text = tr("Recording in this timeslot");
                        break;
                    case kChannelRecord:
                        text = tr("Recording on this channel");
                        break;
                    case kAllRecord:
                        text = tr("Recording all showings");
                        break;
                    case kFindOneRecord:
                        text = tr("Recording one showing");
                        break;
                    case kFindDailyRecord:
                        text = tr("Recording a showing daily");
                        break;
                    case kFindWeeklyRecord:
                        text = tr("Recording a showing weekly");
                        break;
                    case kDontRecord:
                        text = tr("Not allowed to record this showing");
                        break;
                    case kNotRecording:
                        text = tr("Not recording this showing");
                        break;
                    default:
                        text = tr("Error!");
                        break;
                }
                type->SetText(text);
            }

            type = (UITextType *)container->GetType("typerecpriority");
            if (type) {
                type->SetText(QString::number(abs(rectyperecpriority)));
            }
            type = (UITextType *)container->GetType("typesign");
            if (type) {
                if (rectyperecpriority >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("recpriority");
            if (type) {
                if (curitem->recpriority >= 0)
                    type->SetText("+"+QString::number(curitem->recpriority));
                else
                    type->SetText(QString::number(curitem->recpriority));
            }

            type = (UITextType *)container->GetType("recpriorityB");
            if (type) {
                type->SetText(QString::number(abs(progRecPriority)));
            }

            type = (UITextType *)container->GetType("recprioritysign");
            if (type) {
                if (finalRecPriority >= 0)
                    type->SetText("+");
                else
                    type->SetText("-");
            }

            type = (UITextType *)container->GetType("finalrecpriority");
            if (type) {
                if (finalRecPriority >= 0)
                    type->SetText("+"+QString::number(finalRecPriority));
                else
                    type->SetText(QString::number(finalRecPriority));
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
