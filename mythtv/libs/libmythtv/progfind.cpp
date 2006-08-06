/*
        MythProgramFind
        January 19th, 2003
        Updated: 4/8/2003, John Danner
                 Updated code to use new ui.xml file.

        By John Danner
*/

#include <qlabel.h>
#include <qlayout.h>
#include <qdatetime.h>
#include <qtimer.h>
#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstringlist.h>
#include <qcursor.h>
#include <qregexp.h>
#include <unistd.h>
#include <time.h>
#include <cassert>

#include "progfind.h"
#include "proglist.h"
#include "customedit.h"
#include "infostructs.h"
#include "programinfo.h"
#include "oldsettings.h"
#include "tv.h"
#include "guidegrid.h"

using namespace std;

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"

void RunProgramFind(bool thread, bool ggActive)
{
    if (thread)
        qApp->lock();

    gContext->addCurrentLocation("ProgFinder");

    // Language specific progfinder, if needed

    ProgFinder *programFind = NULL;
    if (gContext->GetLanguage() == "ja")
        programFind = new JaProgFinder(gContext->GetMainWindow(),
                                       "program finder", ggActive);
    else
        programFind = new ProgFinder(gContext->GetMainWindow(),
                                     "program finder", ggActive);

    programFind->Initialize();
    programFind->Show();

    if (thread)
    {
        qApp->unlock();

        while (programFind->isVisible())
            usleep(50);
    }
    else
        programFind->exec();

    delete programFind;

    gContext->removeCurrentLocation();

    return;
}

ProgFinder::ProgFinder(MythMainWindow *parent, const char *name, bool gg)
          : MythDialog(parent, name)
{
    curSearch = 10;
    searchCount = 37;
    ggActive = gg;

    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
}

void ProgFinder::Initialize(void)
{
    running = true;
    
    allowkeypress = true;
    inFill = false;
    needFill = false;

    baseDir = gContext->GetInstallPrefix();

    listRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);

    timeFormat = gContext->GetSetting("TimeFormat");
    dateFormat = gContext->GetSetting("DateFormat");

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    theme->LoadTheme(xmldata, "programfind");
    LoadWindow(xmldata);

    LayerSet *container = theme->GetSet("selector");
    assert(container);

    UIListType *ltype = (UIListType *)container->GetType("alphabet");
    if (ltype)
    {
        showsPerListing = ltype->GetItems();
    }

    updateBackground();

    progData = new QString[1];
    listCount = 1;
    if (showsPerListing < 1)
        showsPerListing = 7;
    if (showsPerListing % 2 == 0)
        showsPerListing = showsPerListing + 1;

    inSearch = 0;
    pastInitial = false;

    initData = new QString[(int)(searchCount*showsPerListing)];
    gotInitData = new int[searchCount];
    searchData = new QString[searchCount];

    fillSearchData();

    update_Timer = new QTimer(this);
    connect(update_Timer, SIGNAL(timeout()), SLOT(update_timeout()) );

    getSearchData(curSearch);
    showSearchList();

    update_Timer->start((int)(100));

    setNoErase();
    gContext->addListener(this);

    showInfo = false;
}

ProgFinder::~ProgFinder()
{
    gContext->removeListener(this);

    if (inSearch > 0)
        delete [] progData;
    delete [] searchData;
    delete [] initData;

    delete [] gotInitData;

    delete update_Timer;

    delete theme;
}

void ProgFinder::keyPressEvent(QKeyEvent *e)
{
    if (!allowkeypress)
        return;
    allowkeypress = false;

    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
            cursorUp();
        else if (action == "DOWN")
            cursorDown();
        else if (action == "LEFT")
            cursorLeft();
        else if (action == "RIGHT")
            cursorRight();
        else if (action == "PAGEUP")
            pageUp();
        else if (action == "PAGEDOWN")
            pageDown();
        else if (action == "SELECT" || action == "INFO")
            select();
        else if (action == "CUSTOMEDIT")
            customEdit();
        else if (action == "UPCOMING")
            upcoming();
        else if (action == "DETAILS")
            details();
        else if (action == "MENU" || action == "ESCAPE")
            escape();
        else if (action == "TOGGLERECORD")
            quickRecord();
        else if (action == "4")
            showGuide();
        else
            handled = false;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);

    if (needFill && inSearch == 2)
    {
        do
        {
            needFill = false;
            ProgramInfo *curPick = showData[curShow];
            if (curPick)
                selectShowData(curPick->title, curShow);
        } while (needFill);
    }

    allowkeypress = true;
}

void ProgFinder::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
    {
        if (gContext->GetNumSetting("EPGProgramBar", 1) == 1)
            container->Draw(&tmp, 0, 1);
        else
            container->Draw(&tmp, 0, 0);
    }

    tmp.end();

    setPaletteBackgroundPixmap(bground);
}

void ProgFinder::paintEvent(QPaintEvent *e)
{
    if (inFill)
        return;

    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(listRect))
        updateList(&p);
    if (r.intersects(infoRect))
        updateInfo(&p);
}

void ProgFinder::updateList(QPainter *p)
{
    QRect pr = listRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("alphabet");
        if (ltype)
        {
            if (inSearch == 0)
            {
                ltype->SetItemCurrent((int)(ltype->GetItems() / 2));
                ltype->SetActive(true);
            }
            else
            {
                ltype->SetItemCurrent(-1);
                ltype->SetActive(false);
            }
        }

        ltype = (UIListType *)container->GetType("shows");
        if (ltype)
        {
            if (inSearch == 1)
            {
                ltype->SetItemCurrent((int)(ltype->GetItems() / 2));
                ltype->SetActive(true);
            }
            else
            {
                ltype->SetItemCurrent(-1);
                ltype->SetActive(false);
            }
        }

        ltype = (UIListType *)container->GetType("times");
        if (ltype)
        {
            if (inSearch == 2)
            {
                ltype->SetItemCurrent((int)(ltype->GetItems() / 2));
                ltype->SetActive(true);
            }
            else
            {
                ltype->SetItemCurrent(-1);
                ltype->SetActive(false);
            }
        }

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

void ProgFinder::updateInfo(QPainter *p)
{
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;

    if (inSearch == 0)
    {
        QString title = "";
        QString description = "";
        if (gotInitData[curSearch] == 1)
            container = theme->GetSet("noprograms");
        else
            container = theme->GetSet("help_info_1");
    }
    else if (inSearch == 1)
    {
        container = theme->GetSet("help_info_2");
    }
    else if (inSearch == 2)
    {
        container = theme->GetSet("program_info");
        if (container)
        {
            container->ClearAllText();
            if (gotInitData[curSearch] == 1)
            {
                UITextType *type = (UITextType *)container->GetType("title");
                if (type)
                    type->SetText(tr("No Programs"));
                type = (UITextType *)container->GetType("description");
                if (type)
                    type->SetText(tr("There are no available programs under "
                                     "this search. Please select another "
                                     "search."));
            }
            else
            {
                QMap<QString, QString> infoMap;
                showData[curShow]->ToMap(infoMap);
                container->SetText(infoMap);
            }
        }
    }

    if (container)
        container->Draw(&tmp, 6, 0);

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void ProgFinder::escape()
{
    running = false;
    hide();

    unsetCursor();
    emit accept();
}

void ProgFinder::showGuide()
{
    if (!ggActive)
    {
        QString startchannel = gContext->GetSetting("DefaultTVChannel");
        if (startchannel == "")
            startchannel = "3";
        uint startchanid = 0;
        RunProgramGuide(startchanid, startchannel);
    }
                
    running = false;
    hide();

    unsetCursor();
    emit accept();
}

void ProgFinder::LoadWindow(QDomElement &element)
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
                continue;
            }
        }
    }
}

void ProgFinder::parseContainer(QDomElement &element)
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

void ProgFinder::getInfo(bool toggle)
{
    if (inSearch == 2)
    {
        showInfo = 1;
        ProgramInfo *curPick = showData[curShow];

        if (curPick)
        {
            if (toggle)
                curPick->ToggleRecord();
            else
                curPick->EditRecording();
        }
        else
            return;

        showInfo = 0;

        selectShowData(curPick->title, curShow);

        setActiveWindow();
        setFocus();
    }
}

void ProgFinder::update_timeout()
{
    if (pastInitial == false)
    {
        update_Timer->stop();
        pastInitial = true;
        getInitialProgramData();
    }
    else
    {
        if (inSearch == 0 && gotInitData[curSearch] == 0)
        {
            int cnt = 0;

            for (int j = 0; j < searchCount; j++)
            {
                if (gotInitData[j] > 1)
                    cnt++;
            }

            int amountDone = (int)(100.0 * (float)((float)cnt /
                                   (float)searchCount));
            QString data = QString(" Loading Data...%1% Complete")
                                  .arg(amountDone);

            LayerSet *container = theme->GetSet("selector");
            if (container)
            {
                UIListType *ltype = (UIListType *)container->GetType("shows");
                if (ltype)
                    ltype->SetItemText((int)(showsPerListing / 2), data);
            }
            update(listRect);
        }
    }
}

void ProgFinder::cursorLeft()
{
    inSearch--;
    if (inSearch == -1)
        escape();
    else
    {
        if (inSearch == 0)
            showSearchList();
        else if (inSearch == 1)
        {
            showProgramList();
            clearShowData();
        }
    }
    update(infoRect);
    update(listRect);
}

void ProgFinder::cursorRight()
{
    if (inSearch < 2) {
        inSearch++;
        if (inSearch == 1)
        {
            if (gotInitData[curSearch] == 0)
                getSearchData(curSearch);
            if (gotInitData[curSearch] >= 10)
                selectSearchData();
            if (gotInitData[curSearch] == 1)
            {
                LayerSet *container = theme->GetSet("selector");
                if (container)
                {
                    UIListType *ltype = (UIListType *)container->GetType("shows");
                    if (ltype)
                        ltype->SetItemText((int)(showsPerListing / 2),
                                           tr("       !! No Programs !!"));
                }
                inSearch = 0;
            }
        }
        if (inSearch == 2)
        {
            if (gotInitData[curSearch] > 10)
                selectShowData(progData[curProgram], 0);
            else
                inSearch = 1;
        }
    }
    update(infoRect);
    update(listRect);
}

void ProgFinder::select()
{
    if (inSearch == 2)
        getInfo();
    else
        cursorRight();
}

void ProgFinder::customEdit()
{
    if (inSearch == 2)
    {
        ProgramInfo *curPick = showData[curShow];

        if (!curPick)
            return;

        CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(),
                                        "customedit", curPick);
        ce->exec();
        delete ce;
    }
    else
        cursorRight();
}

void ProgFinder::upcoming()
{
    if (inSearch == 2)
    {
        ProgramInfo *curPick = showData[curShow];

        if (!curPick)
            return;

        ProgLister *pl = new ProgLister(plTitle, curPick->title, "",
                                        gContext->GetMainWindow(), "proglist");
        pl->exec();
        delete pl;
    }
    else
        cursorRight();
}

void ProgFinder::details()
{
    if (inSearch == 2)
    {
        ProgramInfo *curPick = showData[curShow];

        if (!curPick)
            return;

        curPick->showDetails();
    }
    else
        cursorRight();
}

void ProgFinder::quickRecord()
{
    if (inSearch == 2)
        getInfo(true);
    else
        cursorRight();
}

void ProgFinder::pageUp()
{
    if (inSearch == 0)
    {
        curSearch = curSearch - showsPerListing;
        if (curSearch <= -1)
            curSearch = searchCount + curSearch;

        if (gotInitData[curSearch] <= 1)
            clearProgramList();
        else
            showSearchList();
    }
    if (inSearch == 1)
    {
        curProgram = curProgram - showsPerListing;
        if (curProgram <= -1)
            curProgram = listCount + curProgram;

        showProgramList();
    }
    if (inSearch == 2)
    {
        curShow = curShow - showsPerListing;
        if (curShow <= -1)
            curShow = showCount + curShow;

        showShowingList();
    }
}

void ProgFinder::pageDown()
{
    if (inSearch == 0)
    {
        curSearch = curSearch + showsPerListing;

        if (curSearch >= searchCount)
            curSearch = curSearch - searchCount;

        if (gotInitData[curSearch] <= 1)
            clearProgramList();
        else
            showSearchList();
    }
    if (inSearch == 1)
    {
        curProgram = curProgram + showsPerListing;
        if (curProgram >= listCount)
            curProgram = curProgram - listCount;

        showProgramList();
    }
    if (inSearch == 2)
    {
        curShow = curShow + showsPerListing;
        if (curShow >= showCount)
            curShow = curShow - showCount;

        showShowingList();
    }
}

void ProgFinder::cursorUp()
{
    if (inSearch == 0)
    {
        curSearch--;
        if (curSearch == -1)
            curSearch = searchCount - 1;

        if (gotInitData[curSearch] <= 1)
            clearProgramList();
        else
            showSearchList();
    }
    if (inSearch == 1)
    {
        curProgram--;
        if (curProgram == -1)
        {
            curProgram = listCount - 1;
            while (progData[curProgram] == "**!0")
                curProgram--;
        }

        showProgramList();
    }
    if (inSearch == 2)
    {
        curShow--;
        if (curShow == -1)
        {
            curShow = showCount - 1;
            while (!showData[curShow])
                curShow--;
        }

        showShowingList();
    }
}

void ProgFinder::cursorDown()
{
    if (inSearch == 0)
    {
        curSearch++;
        if (curSearch >= searchCount)
            curSearch = 0;

        if (gotInitData[curSearch] <= 1)
            clearProgramList();
        else
            showSearchList();
    }
    if (inSearch == 1)
    {
        if ((curProgram + 1) >= listCount)
            curProgram = -1;

        if (progData[curProgram + 1] != "**!0")
        {
            curProgram++;
            if (curProgram == listCount)
                curProgram = 0;

            showProgramList();
        }
        else
        {
            curProgram = 0;
            showProgramList();
        }
    }
    if (inSearch == 2)
    {
        if ((curShow + 1) >= showCount)
            curShow = -1;

        if (showData[curShow + 1])
        {
            curShow++;
            if (curShow == showCount)
                curShow = 0;

            showShowingList();
        }
        else
        {
            curShow = 0;
            showShowingList();
        }
    }
}

void ProgFinder::showSearchList()
{
    int curLabel = 0;
    int t = 0;
    int tempSearch;
    QString placeHold;

    tempSearch = curSearch;

    LayerSet *container = NULL;
    container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("alphabet");
        if (ltype)
        {
            ltype->ResetList();
            for (int i = (tempSearch - ((showsPerListing - 1) / 2));
                 i < (tempSearch + ((showsPerListing + 1) / 2)); i++)
            {
                t = i;
                if (i < 0)
                    t = i + searchCount;
                if (i >= searchCount)
                    t = i - searchCount;
                if (t < 0)
                    cerr << "MythProgFind: -1 Error in showSearchList()\n";

                if (searchData[t] != NULL)
                {
                    if (curLabel == (int)(showsPerListing / 2))
                        ltype->SetItemText(curLabel, " " + searchData[t] + " ");
                    else
                        ltype->SetItemText(curLabel, " " + searchData[t] + " ");
                }
                else
                    ltype->SetItemText(curLabel, "");
                curLabel++;
            }
        }

        ltype = (UIListType *)container->GetType("shows");
        if (ltype)
        {
            ltype->ResetList();
            if (gotInitData[curSearch] > 1)
            {
                if (update_Timer->isActive() == true)
                    update_Timer->stop();

                curLabel = 0;

                for (int i = (int)(tempSearch * showsPerListing);
                     i < (int)((tempSearch + 1) * showsPerListing); i++)
                {
                    if (initData[i] != NULL)
                    {
                        if (curLabel == (int)(showsPerListing / 2))
                            ltype->SetItemText(curLabel, " " + initData[i] +
                                               " ");
                        else
                            ltype->SetItemText(curLabel, " " + initData[i] +
                                               " ");
                     }
                     else
                         ltype->SetItemText(curLabel, "");
                     curLabel++;
                }
            }
        }
        else
        {
            update_Timer->start((int)250);
        }
    }

    update(listRect);
    update(infoRect);
}

void ProgFinder::showProgramList()
{
    if (gotInitData[curSearch] > 1)
    {
        LayerSet *container = NULL;
        container = theme->GetSet("selector");
        if (container)
        {
            UIListType *ltype = (UIListType *)container->GetType("shows");
            if (ltype)
            {
                int curLabel = 0;
                int t = 0;
                for (int i = (curProgram - ((showsPerListing - 1) / 2));
                        i < (curProgram + ((showsPerListing + 1) / 2)); i++)
                {
                    t = i;
                    if (i < 0)
                        t = i + listCount;
                    if (i >= listCount)
                        t = i - listCount;

                    if (progData[t] != NULL)
                    {
                        if (progData[t] != "**!0")
                        {
                            if (curLabel == (int)(showsPerListing / 2))
                                ltype->SetItemText(curLabel, " " + progData[t]
                                                   + " ");
                            else
                                ltype->SetItemText(curLabel, " " + progData[t]
                                                   + " ");
                        }
                        else
                            ltype->SetItemText(curLabel, "");
                    }
                    else
                        ltype->SetItemText(curLabel, "");
                    curLabel++;
                }
            }
        }
        update(listRect);
    }
}

void ProgFinder::showShowingList()
{
    int curLabel = 0;
    QString tempData = "";

    if (showCount > 0)
    {
        LayerSet *container = NULL;
        container = theme->GetSet("selector");
        if (container)
        {
            UIListType *ltype = (UIListType *)container->GetType("times");
            if (ltype)
            {
                ltype->ResetList();
                int t = 0;
                for (int i = (curShow - ((showsPerListing - 1) / 2));
                     i < (curShow + ((showsPerListing + 1) / 2)); i++)
                {
                    t = i;
                    if (i < 0)
                        t = i + showCount;
                    if (i >= showCount)
                        t = i - showCount;

                    if (showData[t])
                    {
                        ltype->SetItemText(curLabel, " "
                            + showData[t]->startts.toString(dateFormat)
                            + " " + showData[t]->startts.toString(timeFormat));

                        if (showData[t]->recstatus == rsRecording)
                            ltype->EnableForcedFont(curLabel, "recording");
                        else if (showData[t]->recstatus == rsWillRecord)
                            ltype->EnableForcedFont(curLabel, "record");

                    }
                    else
                        ltype->SetItemText(curLabel, "");
                    curLabel++;
                }
            }
        }
    }
    update(infoRect);
    update(listRect);
}

void ProgFinder::selectSearchData()
{
    if (running == false)
        return;

    inFill = true;
    QString thequery;
    QString data;
    MSqlBindings bindings;

    MSqlQuery query(MSqlQuery::InitCon());
    whereClauseGetSearchData(curSearch, thequery, bindings);

    query.prepare(thequery);
    query.bindValues(bindings);
    query.exec();
    
    int rows = query.size();

    if (rows == -1)
    {
        cerr << "MythProgFind: Error executing query! (selectSearchData)\n";
        cerr << "MythProgFind: QUERY = " << thequery.local8Bit() << endl;
        return;
    }

    delete [] progData;

    listCount = 0;

    if (query.isActive() && rows > 0)
    {
        typedef QMap<QString,QString> ShowData;
        ShowData tempList;

        while (query.next())
        {
            if (running == false)
                return;
            data = QString::fromUtf8(query.value(0).toString());

            if (formatSelectedData(data))
            {
                tempList[data.lower()] = data;
                listCount++;
            }
        }

        if (listCount < showsPerListing)
        {
            progData = new QString[showsPerListing];
            for (int i = 0; i < showsPerListing; i++)
                progData[i] = "**!0";
        }
        else
            progData = new QString[(int)listCount];

        int cnt = 0;

        ShowData::Iterator it;
        for (it = tempList.begin(); it != tempList.end(); ++it)
        {
            QString tmpProgData = it.data();
            restoreSelectedData(tmpProgData);

            progData[cnt] = tmpProgData;

            cnt++;
        }
    }

    if (rows < showsPerListing)
        listCount = showsPerListing;

    curProgram = 0;
    inFill = false;
    showProgramList();
}

void ProgFinder::clearProgramList()
{
    if (gotInitData[curSearch] == 0)
    {
        int cnt = 0;
        LayerSet *container = theme->GetSet("selector");
        if (container)
        {
            UIListType *ltype = (UIListType *)container->GetType("shows");
            if (ltype)
            {
                for (int i = 0; i < showsPerListing; i++)
                    ltype->SetItemText(i, "");
            }
        }

        for (int j = 0; j < searchCount; j++)
        {
            if (gotInitData[j] > 0)
                cnt++;
        }

        int amountDone = (int)(100.0 * (float)((float)cnt /
                                               (float)searchCount));

        QString data = QString(" Loading Data...%1% Complete").arg(amountDone);

        if (container)
        {
            UIListType *ltype = (UIListType *)container->GetType("shows");
            if (ltype)
                ltype->SetItemText((int)(showsPerListing / 2), data);
        }
        showSearchList();
    }
    else
    {
        showSearchList();
    }
}

void ProgFinder::clearShowData()
{
    showData.clear();

    LayerSet *container = theme->GetSet("selector");
    if (container)
    {
        UIListType *ltype = (UIListType *)container->GetType("times");
        if (ltype)
        {
            for (int i = 0; i < showsPerListing; i++)
                ltype->SetItemText(i, "");
        }
    }
    update(infoRect);
}

void ProgFinder::selectShowData(QString progTitle, int newCurShow)
{
    if (running == false)
        return;

    inFill = true;
    QDateTime progStart = QDateTime::currentDateTime();

    schedList.FromScheduler();

    MSqlBindings bindings;
    QString querystr = "WHERE program.title = :TITLE "
                       "  AND program.endtime > :ENDTIME ";
    bindings[":TITLE"] = progTitle.utf8();
    bindings[":ENDTIME"] = progStart.toString("yyyy-MM-ddThh:mm:50");

    showData.FromProgram(querystr, bindings, schedList);

    showCount = showData.count();
    if (showCount < showsPerListing)
        showCount = showsPerListing;

    curShow = newCurShow;
    inFill = false;
    showShowingList();
}

void ProgFinder::getInitialProgramData()
{
    getAllProgramData();
}

void ProgFinder::getSearchData(int charNum)
{
    if (running == false)
        return;

    int cnts = 0;
    int dataNum = 0;
    int startPlace = 0;
    QString thequery;
    QString data;
    MSqlBindings bindings;

    whereClauseGetSearchData(charNum, thequery, bindings);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(thequery);
    query.bindValues(bindings);
    query.exec();

    int rows = query.size();

    if (rows == -1)
    {
        cerr << "MythProgFind: Error executing query! (getSearchData)\n";
        cerr << "MythProgFind: QUERY = " << thequery << endl;
        return;
    }

    startPlace = (int)(charNum*showsPerListing);
    dataNum = (int)(showsPerListing / 2);

    if (query.isActive() && rows > 0)
    {
        typedef QMap<QString,QString> ShowData;
        ShowData tempList;

        while (query.next())
        {
            data = QString::fromUtf8(query.value(0).toString());

            if (formatSelectedData(data, charNum))
            {
                tempList[data.lower()] = data;
                cnts++;
            }
        }

        int cnt = 0;
        int dNum = 0;

        ShowData::Iterator it;
        for (it = tempList.begin(); it != tempList.end(); ++it)
        {
            if (cnt <= (int)(showsPerListing / 2))
            {
                data = it.data();
                restoreSelectedData(data);
                initData[startPlace + dataNum] = data;
                dataNum++;
            }

            if (cnt >= (cnts - (int)(showsPerListing / 2)) &&
                rows >= showsPerListing)
            {
                data = it.data();
                restoreSelectedData(data);
                initData[startPlace + dNum] = data;
                dNum++;
            }

            cnt++;
        }
    }

    if (cnts == 0)
        gotInitData[charNum] = 1;
    else
    {
        gotInitData[charNum] = 10 + cnts;
    }

    if (charNum == curSearch)
        showSearchList();
}

void ProgFinder::fillSearchData(void)
{
    int curLabel = 0;

    for (int charNum = 48; charNum < 91; charNum++)
    {
        if (charNum == 58)
            charNum = 65;

        gotInitData[curLabel] = 0;
        searchData[curLabel] = (char)charNum;
        curLabel++;
    }

    gotInitData[curLabel] = 0;
    searchData[curLabel] = '@';
}

void ProgFinder::whereClauseGetSearchData(int charNum, QString &where,
                                          MSqlBindings &bindings)
{
    QDateTime progStart = QDateTime::currentDateTime();

    if (searchData[charNum].contains('@'))
    {
        where = "SELECT DISTINCT title FROM program WHERE ( "
                   "title NOT REGEXP '^[A-Z0-9]' AND "
                   "title NOT REGEXP '^The [A-Z0-9]' AND "
                   "title NOT REGEXP '^A [A-Z0-9]' AND "
                   "starttime > :STARTTIME ) ORDER BY title;";
        bindings[":STARTTIME"] = progStart.toString("yyyy-MM-ddThh:mm:50");
    }
    else
    {
        QString one = searchData[charNum] + "%";
        QString two = QString("The ") + one;
        QString three = QString("A ") + one;
        where = "SELECT DISTINCT title "
                "FROM program "
                "WHERE ( title LIKE :ONE OR title LIKE :TWO "
                "        OR title LIKE :THREE ) "
                "AND starttime > :STARTTIME "
                "ORDER BY title;";
        bindings[":ONE"] = one.utf8();
        bindings[":TWO"] = two.utf8();
        bindings[":THREE"] = three.utf8();
        bindings[":STARTTIME"] = progStart.toString("yyyy-MM-ddThh:mm:50");
    }
}

bool ProgFinder::formatSelectedData(QString& data)
{
    bool retval = true;

    if (searchData[curSearch] == "T" || searchData[curSearch] == "A")
    {
        if (data.left(5) == "The T" && searchData[curSearch] == "T")
            data = data.mid(4) + ", The";
        else if (data.left(5) == "The A" && searchData[curSearch] == "A")
            data = data.mid(4) + ", The";
        else if (data.left(3) == "A T" && searchData[curSearch] == "T")
            data = data.mid(2) + ", A";
        else if (data.left(3) == "A A" && searchData[curSearch] == "A")
             data = data.mid(2) + ", A";
        else if (data.left(4) != "The " && data.left(2) != "A ")
        {
             // nothing, use as is
        }
        else
        {
            // don't add
            retval = false;
        }
    }
    else
    {
        if (data.left(4) == "The ")
            data = data.mid(4) + ", The";
        if (data.left(2) == "A ")
            data = data.mid(2) + ", A";
    }

    return retval;
}

bool ProgFinder::formatSelectedData(QString& data, int charNum)
{
    bool retval = true;

    if (charNum == 29 || charNum == 10)
    {
        if (data.left(5) == "The T" && charNum == 29)
            data = data.mid(4) + ", The";
        else if (data.left(5) == "The A" && charNum == 10)
            data = data.mid(4) + ", The";
        else if (data.left(3) == "A T" && charNum == 29)
            data = data.mid(2) + ", A";
        else if (data.left(3) == "A A" && charNum == 10)
            data = data.mid(2) + ", A";
        else if (data.left(4) != "The " && data.left(2) != "A ")
        {
            // use as is
        }
        else
        {
            // don't add
            retval = false;
        }
    }
    else
    {
        if (data.left(4) == "The ")
            data = data.mid(4) + ", The";
        if (data.left(2) == "A ")
            data = data.mid(2) + ", A";
    }

    return retval;
}

void ProgFinder::restoreSelectedData(QString &data)
{
    if (data.right(5) == ", The")
        data = "The " + data.left(data.length() - 5);
    if (data.right(3) == ", A")
        data = "A " + data.left(data.length() - 3);
}

void ProgFinder::getAllProgramData()
{
    getSearchData(8);
    getSearchData(9);
    getSearchData(11);
    getSearchData(12);

    for (int charNum = 0; charNum < searchCount; charNum++)
    {
        if (charNum == 8)
            charNum = 13;

        getSearchData(charNum);
    }
}

void ProgFinder::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message == "SCHEDULE_CHANGE")
        {
            if (inSearch == 2)
            {
                if (!allowkeypress)
                {
                    needFill = true;
                    return;
                }

                allowkeypress = false;

                do
                {
                    needFill = false;
                    ProgramInfo *curPick = showData[curShow];
                    if (curPick)
                        selectShowData(curPick->title, curShow);
                } while (needFill);

                allowkeypress = true;
            }
        }
    }
}


// Japanese specific program finder

// japanese HIRAGANA list and more
const char* JaProgFinder::searchChars[] = {
    "あ", "か", "さ", "た", "な", "は", "ま", "や", "ら", "わ",
    "英", "数",
    0,
};

JaProgFinder::JaProgFinder(MythMainWindow *parent, const char *name, bool gg)
            : ProgFinder(parent, name, gg)
{
    for (numberOfSearchChars = 0; searchChars[numberOfSearchChars];
         numberOfSearchChars++)
         ;

    searchCount = numberOfSearchChars;
    curSearch = 0;
}

void JaProgFinder::fillSearchData()
{
    int curLabel = 0;
    for (int charNum = 0; charNum < searchCount; charNum++)
    {
        gotInitData[curLabel] = 0;
        searchData[curLabel] = QString::fromUtf8(searchChars[charNum]);
        curLabel++;
    }
}

// search title_pronounce
// we hope japanese HIRAGANA and alphabets, numerics is inserted
// these character must required ZENKAKU
// because query work not fine, if mysql's default charset latin1
void JaProgFinder::whereClauseGetSearchData(int charNum, QString &where,
                                            MSqlBindings &bindings)
{
    QDateTime progStart = QDateTime::currentDateTime();

    where = "SELECT DISTINCT title FROM program ";

    switch (charNum) {
    case 0:
        where += "WHERE ( title_pronounce >= 'あ' AND title_pronounce <= 'お') ";
        break;
    case 1:
        where += "WHERE ( title_pronounce >= 'か' AND title_pronounce <= 'ご') ";
        break;
    case 2:
        where += "WHERE ( title_pronounce >= 'さ' AND title_pronounce <= 'そ') ";
        break;
    case 3:
        where += "WHERE ( title_pronounce >= 'た' AND title_pronounce <= 'ど') ";
        break;
    case 4:
        where += "WHERE ( title_pronounce >= 'な' AND title_pronounce <= 'の') ";
        break;
    case 5:
        where += "WHERE ( title_pronounce >= 'は' AND title_pronounce <= 'ぽ') ";
        break;
    case 6:
        where += "WHERE ( title_pronounce >= 'ま' AND title_pronounce <= 'も') ";
        break;
    case 7:
        where += "WHERE ( title_pronounce >= 'や' AND title_pronounce <= 'よ') ";
        break;
    case 8:
        where += "WHERE ( title_pronounce >= 'ら' AND title_pronounce <= 'ろ') ";
        break;
    case 9:
        where += "WHERE ( title_pronounce >= 'わ' AND title_pronounce <= 'ん') ";
        break;
    case 10:
        where += "WHERE ( title_pronounce >= 'Ａ' AND title_pronounce <= 'ｚ') ";
        break;
    case 11:
        where += "WHERE ( title_pronounce >= '０' AND title_pronounce <= '９') ";
        break;
    }

    where += "AND starttime > :STARTTIME ORDER BY title_pronounce;";
    bindings[":STARTTIME"] = progStart.toString("yyyy-MM-ddThh:mm:50");
}


bool JaProgFinder::formatSelectedData(QString& data)
{
    (void)data;
    return true;
}

bool JaProgFinder::formatSelectedData(QString& data, int charNum)
{
    (void)data;
    (void)charNum;
    return true;
}

void JaProgFinder::restoreSelectedData(QString& data)
{
    (void)data;
}

void JaProgFinder::getAllProgramData()
{
    for (int charNum = 0; charNum < searchCount; charNum++)
    {
        getSearchData(charNum);
    }
}
