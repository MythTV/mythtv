#include <qapplication.h>
#include <qpainter.h>
#include <qfont.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <math.h>
#include <qcursor.h>
#include <qapplication.h>
#include <qimage.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qdatetime.h>
#include <qvgroupbox.h>
#include <qheader.h>
#include <qrect.h>

#include <unistd.h>
#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "mythdbcon.h"
#include "guidegrid.h"
#include "infostructs.h"
#include "programinfo.h"
#include "oldsettings.h"
#include "tv_play.h"
#include "tv_rec.h"
#include "progfind.h"
#include "proglist.h"
#include "customedit.h"
#include "util.h"
#include "remoteutil.h"

bool RunProgramGuide(uint &chanid, QString &channum,
                     bool thread, TV *player,
                     bool allowsecondaryepg)
{
    bool channel_changed = false;

    if (thread)
        qApp->lock();

    gContext->addCurrentLocation("GuideGrid");

    GuideGrid *gg = new GuideGrid(gContext->GetMainWindow(),
                                  chanid, channum,
                                  player, allowsecondaryepg, "guidegrid");

    gg->Show();

    if (thread)
    {
        qApp->unlock();

        while (gg->isVisible())
            usleep(50);
    }
    else
        gg->exec();

    if (chanid != gg->GetChanID())
    {
        chanid  = gg->GetChanID();
        channum = gg->GetChanNum();
        channel_changed = true;
    }

    if (thread)
        qApp->lock();

    delete gg;

    gContext->removeCurrentLocation();

    if (thread)
        qApp->unlock();

    return channel_changed;
}

GuideGrid::GuideGrid(MythMainWindow *parent,
                     uint chanid, QString channum,
                     TV *player, bool allowsecondaryepg,
                     const char *name)
         : MythDialog(parent, name)
{
    desiredDisplayChans = DISPLAY_CHANS = 6;
    DISPLAY_TIMES = 30;
    int maxchannel = 0;
    m_currentStartChannel = 0;

    m_player = player;

    m_context = 0;

    fullRect = QRect(0, 0, size().width(), size().height());
    dateRect = QRect(0, 0, 0, 0);
    jumpToChannelRect = QRect(0, 0, 0, 0);
    channelRect = QRect(0, 0, 0, 0);
    timeRect = QRect(0, 0, 0, 0);
    programRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    curInfoRect = QRect(0, 0, 0, 0);
    videoRect = QRect(0, 0, 0, 0);

    jumpToChannelEnabled = gContext->GetNumSetting("EPGEnableJumpToChannel", 0);
    jumpToChannelActive = false;
    jumpToChannelHasRect = false; // by default, we assume there is no specific region for jumpToChannel in the theme
    jumpToChannelTimer = new QTimer(this);
    connect(jumpToChannelTimer, SIGNAL(timeout()), SLOT(jumpToChannelTimeout()));
    
    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (m_player && m_player->IsRunning() && allowsecondaryepg)
        theme->LoadTheme(xmldata, "programguide-video");
    else 
        theme->LoadTheme(xmldata, "programguide");

    LoadWindow(xmldata);

    if (m_player && m_player->IsRunning() && !allowsecondaryepg)
        videoRect = QRect(0, 0, 1, 1);

    showFavorites = gContext->GetNumSetting("EPGShowFavorites", 0);
    gridfilltype = gContext->GetNumSetting("EPGFillType", UIGuideType::Alpha);
    if (gridfilltype < (int)UIGuideType::Alpha)
    { // update old settings to new fill types
        if (gridfilltype == 5)
            gridfilltype = UIGuideType::Dense;
        else
            gridfilltype = UIGuideType::Alpha;
            
        gContext->SaveSetting("EPGFillType", gridfilltype);
    }

    scrolltype = gContext->GetNumSetting("EPGScrollType", 1);

    selectChangesChannel = gContext->GetNumSetting("SelectChangesChannel", 0);
    selectRecThreshold = gContext->GetNumSetting("SelChangeRecThreshold", 16);

    LayerSet *container = NULL;
    container = theme->GetSet("guide");
    if (container)
    {
        UIGuideType *type = (UIGuideType *)container->GetType("guidegrid");
        if (type) 
        {
            type->SetFillType(gridfilltype);
            type->SetShowCategoryColors(
                   gContext->GetNumSetting("EPGShowCategoryColors", 1));
            type->SetShowCategoryText(
                   gContext->GetNumSetting("EPGShowCategoryText", 1));
        }
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }

    timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    QTime new_time = QTime::currentTime();
    QString curTime = new_time.toString(timeformat);

    container = theme->GetSet("current_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("time");
        if (type)
            type->SetText(curTime);
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }
    
    container = theme->GetSet("program_info");
    if (container)
    {
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }

    channelOrdering = gContext->GetSetting("ChannelOrdering", "channum + 0");
    dateformat = gContext->GetSetting("ShortDateFormat", "ddd d");
    unknownTitle = gContext->GetSetting("UnknownTitle", "Unknown");
    unknownCategory = gContext->GetSetting("UnknownCategory", "Unknown");
    currentTimeColor = gContext->GetSetting("EPGCurrentTimeColor", "red");
    channelFormat = gContext->GetSetting("ChannelFormat", "<num> <sign>");
    channelFormat.replace(" ", "\n");

    UIBarType *type = NULL;
    container = theme->GetSet("chanbar");

    int dNum = gContext->GetNumSetting("chanPerPage", 8);

    if (m_player && m_player->IsRunning() && allowsecondaryepg)
        dNum = dNum * 2 / 3 + 1;

    desiredDisplayChans = DISPLAY_CHANS = dNum;
    if (container)
    {
        type = (UIBarType *)container->GetType("chans");
        if (type)
            type->SetSize(dNum);
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }

    container = theme->GetSet("timebar");
    dNum = gContext->GetNumSetting("timePerPage", 5);
    if (dNum > 5)
        dNum = 5;
    DISPLAY_TIMES = 6 * dNum;

    if (container)
    {
        type = (UIBarType *)container->GetType("times");
        if (type)
            type->SetSize(dNum);
        if (gridfilltype == UIGuideType::Eco)    
            container->SetDrawFontShadow(false);
    }
    m_originalStartTime = QDateTime::currentDateTime();

    int secsoffset = -((m_originalStartTime.time().minute() % 30) * 60 +
                        m_originalStartTime.time().second());
    m_currentStartTime = m_originalStartTime.addSecs(secsoffset);
    startChanID  = chanid;
    startChanNum = channum;

    m_currentRow = (int)(desiredDisplayChans / 2);
    m_currentCol = 0;

    for (int y = 0; y < MAX_DISPLAY_CHANS; y++)
        m_programs[y] = NULL;

    for (int x = 0; x < MAX_DISPLAY_TIMES; x++)
    {
        m_timeInfos[x] = NULL;
        for (int y = 0; y < MAX_DISPLAY_CHANS; y++)
            m_programInfos[y][x] = NULL;
    }

    //MythTimer clock = QTime::currentTime();
    //clock.start();
    fillTimeInfos();
    //int filltime = clock.elapsed();
    //clock.restart();
    fillChannelInfos();
    maxchannel = max((int)m_channelInfos.size() - 1, 0);
    setStartChannel((int)(m_currentStartChannel) - 
                    (int)(desiredDisplayChans / 2));
    DISPLAY_CHANS = min(DISPLAY_CHANS, maxchannel + 1);

    //int fillchannels = clock.elapsed();
    //clock.restart();
    m_recList.FromScheduler();
    fillProgramInfos();
    //int fillprogs = clock.elapsed();

    timeCheck = NULL;
    timeCheck = new QTimer(this);
    connect(timeCheck, SIGNAL(timeout()), SLOT(timeout()) );
    timeCheck->start(200);

    selectState = false;

    updateBackground();

    setNoErase();
    gContext->addListener(this);
    
    keyDown = false;
    setFocusPolicy(QWidget::ClickFocus); //get keyRelease events after refocus
}

GuideGrid::~GuideGrid()
{
    gContext->removeListener(this);
    for (int x = 0; x < MAX_DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x])
            delete m_timeInfos[x];
    }

    for (int y = 0; y < MAX_DISPLAY_CHANS; y++)
    {
        if (m_programs[y])
            delete m_programs[y];
    }

    m_channelInfos.clear();

    delete theme;
}

void GuideGrid::keyPressEvent(QKeyEvent *e)
{
    // keyDown limits keyrepeats to prevent continued scrolling
    // after key is released. Note: Qt's keycompress should handle
    // this but will fail with fast key strokes and keyboard repeat
    // enabled. Keys will not be marked as autorepeat and flood buffer.
    // setFocusPolicy(QWidget::ClickFocus) in constructor is important 
    // or keyRelease events will not be received after a refocus.
    
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);

    if (actions.size() > 0 && keyDown && actions[0] != "ESCAPE")
        return; //check for Escape in case something goes wrong 
                //with KeyRelease events, shouldn't happen.

    if (e->key() != Key_Control && e->key() != Key_Shift && 
        e->key() != Key_Meta && e->key() != Key_Alt)
        keyDown = true;    

    if (e->state() == Qt::ControlButton)
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "LEFT")
                pageLeft();
            else if (action == "RIGHT")
                pageRight();
            else if (action == "UP")
                pageUp();
            else if (action == "DOWN")
                pageDown();
            else
                handled = false;
        }
        handled = true;
    }
    else
    {
        // We want to handle jump to channel before everything else
        // The reason is because the number keys could be mapped to
        // other things. If this is the case, then the jump to channel
        // will not work correctly.
        if (jumpToChannelEnabled) 
        {
            int digit;
            if (jumpToChannelGetInputDigit(actions, digit)) 
            {
                jumpToChannelDigitPress(digit);
                // Set handled = true here so that it won't process any
                // more actions.
                handled = true;
            }
        }        
	      
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "LEFT")
                cursorLeft();
            else if (action == "RIGHT")
                cursorRight();
            else if (action == "DOWN")
                cursorDown();
            else if (action == "UP")
                cursorUp();
            else if (action == "PAGEUP")
                pageUp();
            else if (action == "PAGEDOWN")
                pageDown();
            else if (action == "PAGELEFT")
                pageLeft();
            else if (action == "PAGERIGHT")
                pageRight();
            else if (action == "DAYLEFT")
                dayLeft();
            else if (action == "DAYRIGHT")
                dayRight();
            else if (jumpToChannelEnabled && jumpToChannelActive &&
                     (action == "ESCAPE"))
                jumpToChannelCancel();
            else if (jumpToChannelEnabled && jumpToChannelActive &&
                     (action == "DELETE"))
                jumpToChannelDeleteLastDigit();
            else if (action == "NEXTFAV" || action == "4")
                toggleGuideListing();
            else if (action == "6")
                showProgFinder();
            else if (action == "MENU")
                enter();                    
            else if (action == "ESCAPE")
                escape();
            else if (action == "SELECT")
            {                         
                if (m_player && selectChangesChannel)
                {
                    // See if this show is far enough into the future that it's probable
                    // that the user wanted to schedule it to record instead of changing the channel.
                    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];
                    if (pginfo && (pginfo->title != unknownTitle) &&
                        ((pginfo->SecsTillStart() / 60) >= selectRecThreshold))
                    {
                        editRecording();
                    }
                    else
                    {
                        enter();
                    }
                }
                else
                    editRecording();
            }
            else if (action == "INFO")
                editScheduled();
            else if (action == "CUSTOMEDIT")
                customEdit();
            else if (action == "UPCOMING")
                upcoming();
            else if (action == "DETAILS")
                details();
            else if (action == "TOGGLERECORD")
                quickRecord();
            else if (action == "TOGGLEFAV")
                toggleChannelFavorite();
            else if (action == "CHANUPDATE")
                channelUpdate();
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void GuideGrid::keyReleaseEvent(QKeyEvent *e)
{
    // note: KeyRelease events may not reflect the released 
    // key if delayed (key==0 instead).
    (void)e;
    keyDown = false;
}

void GuideGrid::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
    if (container)
        container->Draw(&tmp, 0, 0);

    tmp.end();

    setPaletteBackgroundPixmap(bground);
}

void GuideGrid::LoadWindow(QDomElement &element)
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

void GuideGrid::parseContainer(QDomElement &element)
{
    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);

    if (name.lower() == "guide")
        programRect = area;
    if (name.lower() == "program_info")
        infoRect = area;
    if (name.lower() == "chanbar")
        channelRect = area;
    if (name.lower() == "timebar")
        timeRect = area;
    if (name.lower() == "date_info")
        dateRect = area;
    if (name.lower() == "jumptochannel") {
        jumpToChannelRect = area;
        jumpToChannelHasRect = true;
    }
    if (name.lower() == "current_info")
        curInfoRect = area;
    if (name.lower() == "current_video")
        videoRect = area;
}

QString GuideGrid::GetChanNum(void)
{
    if (m_channelInfos.empty() || !selectState)
        return "";

    uint idx = (m_currentRow + m_currentStartChannel) % m_channelInfos.size();
    return m_channelInfos[idx].chanstr;
}

uint GuideGrid::GetChanID(void)
{
    if (m_channelInfos.empty() || !selectState)
        return 0;

    uint idx = (m_currentRow + m_currentStartChannel) % m_channelInfos.size();
    return m_channelInfos[idx].chanid;
}

void GuideGrid::timeout()
{
    timeCheck->changeInterval((int)(60 * 1000));
    QTime new_time = QTime::currentTime();
    QString curTime = new_time.toString(timeformat);

    LayerSet *container = NULL;
    container = theme->GetSet("current_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("time");
        if (type)
            type->SetText(curTime);
    }

    if (m_player)
    {
        if (m_player->IsRunning() == true && videoRect.width() > 0 &&
            videoRect.height() > 0)
            m_player->EmbedOutput(this->winId(), videoRect.x(), videoRect.y(),
                                  videoRect.width(), videoRect.height());
    }

    fillProgramInfos();
    repaint(programRect, false);
    repaint(curInfoRect, false);
}

void GuideGrid::fillChannelInfos(bool gotostartchannel)
{
    MSqlQuery query(MSqlQuery::InitCon());

    m_channelInfos.clear();

    if (showFavorites)
    {
        query.prepare(
            "SELECT channel.channum, channel.callsign, "
            "       channel.icon,    channel.chanid, "
            "       favorites.favid, channel.name "
            "FROM videosource, cardinput, favorites, channel "
            "WHERE channel.chanid       = favorites.chanid     AND "
            "      visible              = 1                    AND "
            "      channel.sourceid     = videosource.sourceid AND "
            "      videosource.sourceid = cardinput.sourceid "
            "GROUP BY channum, callsign "
            "ORDER BY " + channelOrdering);

        if (query.exec() && query.isActive())
            showFavorites = query.size();
        else
            MythContext::DBError("fillChannelInfos -- favorites", query);
    }

    if (!showFavorites)
    {
        query.prepare(
            "SELECT channel.channum, channel.callsign, "
            "       channel.icon,    channel.chanid, "
            "       favorites.favid, channel.name "
            "FROM videosource, cardinput, "
            "       channel LEFT JOIN favorites "
            "       ON favorites.chanid = channel.chanid "
            "WHERE visible              = 1                    AND "
            "      channel.sourceid     = videosource.sourceid AND "
            "      videosource.sourceid = cardinput.sourceid "
            "GROUP BY channum, callsign "
            "ORDER BY " + channelOrdering);

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("fillChannelInfos -- all connected", query);
            return;
        }
    }
 
    bool startingset = false;
    while (query.next())
    {
        ChannelInfo val;

        val.chanstr  = query.value(0).toString();
        val.chanid   = query.value(3).toInt();

        // validate the channum and chanid, skip channel if these are invalid
        if (val.chanstr == "" || !val.chanid)
            continue;

        // fill in the rest of the data...
        val.callsign = QString::fromUtf8(query.value(1).toString());
        val.iconpath = query.value(2).toString();
        val.favid    = query.value(4).toInt();
        val.channame = QString::fromUtf8(query.value(5).toString());
        val.iconload = false;

        // set starting channel index if it hasn't been set
        bool match   = gotostartchannel && !startingset;
        match &= (startChanID)  ? val.chanid  == (int)startChanID : true;
        match &= (!startChanID) ? val.chanstr == startChanNum     : true;
        if (match)
        {
            m_currentStartChannel = m_channelInfos.size();
            startingset = true;
        }
                
        // add the new channel to the list
        m_channelInfos.push_back(val);
    }
    // set starting channel index to 0 if it hasn't been set
    if (gotostartchannel)
        m_currentStartChannel = (startingset) ? m_currentStartChannel : 0;

    if (m_channelInfos.empty())
    {
        VERBOSE(VB_IMPORTANT, "GuideGrid: "
                "\n\t\t\tYou don't have any channels defined in the database."
                "\n\t\t\tGuide grid will have nothing to show you.");
    }
}

void GuideGrid::fillTimeInfos()
{
    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x])
            delete m_timeInfos[x];
        m_timeInfos[x] = NULL;
    }

    QDateTime t = m_currentStartTime;
    int cnt = 0;
 
    LayerSet *container = NULL;
    UIBarType *type = NULL;
    container = theme->GetSet("timebar");
    if (container)
        type = (UIBarType *)container->GetType("times");
   
    firstTime = m_currentStartTime;
    lastTime = firstTime.addSecs(DISPLAY_TIMES * 60 * 4);

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        int mins = t.time().minute();
        mins = 5 * (mins / 5);
        if (mins % 30 == 0)
        {
            TimeInfo *timeinfo = new TimeInfo;

            int hour = t.time().hour();
            timeinfo->hour = hour;
            timeinfo->min = mins;

            timeinfo->usertime = QTime(hour, mins).toString(timeformat);

            m_timeInfos[x] = timeinfo;
            if (type)
                type->SetText(cnt, timeinfo->usertime);
            cnt++;
        }

        t = t.addSecs(5 * 60);
    }
    m_currentEndTime = t;
}

void GuideGrid::fillProgramInfos(void)
{
    LayerSet *container = NULL;
    UIGuideType *type = NULL;
    container = theme->GetSet("guide");
    if (container)
    {
        type = (UIGuideType *)container->GetType("guidegrid");
        if (type)
        {
            type->SetWindow(this);
            type->SetScreenLocation(programRect.topLeft());
            type->SetNumRows(DISPLAY_CHANS);
            type->ResetData();
        }
    }

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        fillProgramRowInfos(y);
    }
}

void GuideGrid::fillProgramRowInfos(unsigned int row)
{
    LayerSet *container = NULL;
    container = theme->GetSet("guide");
    UIGuideType *type = NULL;
    if (container)
        type = (UIGuideType *)container->GetType("guidegrid");

    if (type)
        type->ResetRow(row);
 
    ProgramList *proglist;
    ProgramInfo *program;
    ProgramInfo *proginfo = NULL;

    if (m_programs[row])
        delete m_programs[row];
    m_programs[row] = NULL;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        m_programInfos[row][x] = NULL;
    }

    int chanNum = row + m_currentStartChannel;
    if (chanNum >= (int) m_channelInfos.size())
        chanNum -= (int) m_channelInfos.size();
    if (chanNum >= (int) m_channelInfos.size())
        return;

    if (chanNum < 0)
        chanNum = 0;

    m_programs[row] = proglist = new ProgramList();

    MSqlBindings bindings;
    QString querystr = "WHERE program.chanid = :CHANID "
                       "  AND program.endtime >= :STARTTS "
                       "  AND program.starttime <= :ENDTS "
                       "  AND program.manualid = 0 ";
    bindings[":CHANID"] = m_channelInfos[chanNum].chanid;
    bindings[":STARTTS"] = m_currentStartTime.toString("yyyy-MM-ddThh:mm:00");
    bindings[":ENDTS"] = m_currentEndTime.toString("yyyy-MM-ddThh:mm:00");

    proglist->FromProgram(querystr, bindings, m_recList);

    QDateTime ts = m_currentStartTime;

    if (type)
    {
        QDateTime tnow = QDateTime::currentDateTime();
        int progPast = 0;
        if (tnow > m_currentEndTime)
            progPast = 100;
        else if (tnow < m_currentStartTime)
            progPast = 0;
        else
        {
            int played = m_currentStartTime.secsTo(tnow);
            int length = m_currentStartTime.secsTo(m_currentEndTime);
            if (length)
                progPast = played * 100 / length;
        }

        type->SetProgPast(progPast);
    }

    program = proglist->first();
    QPtrList<ProgramInfo> unknownlist;
    bool unknown = false;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (program && (ts >= (*program).endts))
        {
            program = proglist->next();
        }

        if ((!program) || (ts < (*program).startts))
        {
            if (unknown)
            {
                proginfo->spread++;
                proginfo->endts = proginfo->endts.addSecs(5 * 60);
            }
            else
            {
                proginfo = new ProgramInfo;
                unknownlist.append(proginfo);
                proginfo->title = unknownTitle;
                proginfo->category = unknownCategory;
                proginfo->startCol = x;
                proginfo->spread = 1;
                proginfo->startts = ts;
                proginfo->endts = proginfo->startts.addSecs(5 * 60);
                unknown = true;
            }
        }
        else
        {
            if (proginfo == &*program)
            {
                proginfo->spread++;
            }
            else
            {
                proginfo = &*program;
                proginfo->startCol = x;
                proginfo->spread = 1;
                unknown = false;
            }
        }
        m_programInfos[row][x] = proginfo;
        ts = ts.addSecs(5 * 60);
    }

    for (proginfo = unknownlist.first(); proginfo; 
         proginfo = unknownlist.next())
    {
        proglist->append(proginfo);
    }

    int ydifference = programRect.height() / DISPLAY_CHANS;
    int xdifference = programRect.width() / DISPLAY_TIMES;

    int arrow = 0;
    int cnt = 0;
    int spread = 1;
    QDateTime lastprog; 
    QRect tempRect;
    bool isCurrent = false;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        proginfo = m_programInfos[row][x];
        if (!proginfo)
            continue;

        spread = 1;
        if (proginfo->startts != lastprog)
        {
            arrow = 0;
            if (proginfo->startts < firstTime.addSecs(-300))
                arrow = arrow + 1;
            if (proginfo->endts > lastTime.addSecs(2100))
                arrow = arrow + 2;

            if (proginfo->spread != -1)
            {
                spread = proginfo->spread;
            }
            else
            {
                for (int z = x + 1; z < DISPLAY_TIMES; z++)
                {
                    ProgramInfo *test = m_programInfos[row][z];
                    if (test && test->startts == proginfo->startts)
                        spread++;
                }
                proginfo->spread = spread;
                proginfo->startCol = x;

                for (int z = x + 1; z < x + spread; z++)
                {
                    ProgramInfo *test = m_programInfos[row][z];
                    if (test)
                    {
                        test->spread = spread;
                        test->startCol = x;
                    }
                }
            }

            tempRect = QRect((int)(x * xdifference), (int)(row * ydifference),
                            (int)(xdifference * proginfo->spread), ydifference);

            if (m_currentRow == (int)row && (m_currentCol >= x) &&
                (m_currentCol < (x + spread)))
                isCurrent = true;
            else 
                isCurrent = false;

            if (type)
            {
                int recFlag;
                switch (proginfo->rectype) 
                {
                case kSingleRecord:
                    recFlag = 1;
                    break;
                case kTimeslotRecord:
                    recFlag = 2;
                    break;
                case kChannelRecord:
                    recFlag = 3;
                    break;
                case kAllRecord:
                    recFlag = 4;
                    break;
                case kWeekslotRecord:
                    recFlag = 5;
                    break;
                case kFindOneRecord:
                case kFindDailyRecord:
                case kFindWeeklyRecord:
                    recFlag = 6;
                    break;
                case kOverrideRecord:
                case kDontRecord:
                    recFlag = 7;
                    break;
                case kNotRecording:
                default:
                    recFlag = 0;
                    break;
                }

                int recStat;
                if (proginfo->recstatus == rsConflict ||
                    proginfo->recstatus == rsOffLine)
                    recStat = 2;
                if (proginfo->recstatus <= rsWillRecord)
                    recStat = 1;
                else
                    recStat = 0;

                type->SetProgramInfo(row, cnt, tempRect, proginfo->title,
                                     proginfo->category, arrow, recFlag, 
                                     recStat, isCurrent);

                cnt++;
            }
        }

        lastprog = proginfo->startts;
    } 
}

void GuideGrid::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message == "SCHEDULE_CHANGE")
        {
            m_recList.FromScheduler();
            fillProgramInfos();
            update(fullRect);
        }
    }
}

void GuideGrid::paintEvent(QPaintEvent *e)
{
    qApp->lock();

    QRect r = e->rect();
    QPainter p(this);

    if (r.intersects(infoRect))
        paintInfo(&p);
    if (r.intersects(dateRect) && (jumpToChannelHasRect || (!jumpToChannelHasRect && !jumpToChannelActive)))
        paintDate(&p);
    if (r.intersects(channelRect))
        paintChannels(&p);
    if (r.intersects(timeRect))
        paintTimes(&p);
    if (r.intersects(programRect))
        paintPrograms(&p);
    if (r.intersects(curInfoRect))
        paintCurrentInfo(&p);

    // if jumpToChannel has its own rect, use that; otherwise use the date's rect
    if ((jumpToChannelHasRect && r.intersects(jumpToChannelRect)) ||
        (!jumpToChannelHasRect && r.intersects(dateRect)))
        paintJumpToChannel(&p);

    if (r.intersects(videoRect) && m_player)
    {
        timeCheck->changeInterval((int)(200));
    }

    qApp->unlock();
}

void GuideGrid::paintDate(QPainter *p)
{
    QRect dr = dateRect;
    QPixmap pix(dr.size());
    pix.fill(this, dr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    container = theme->GetSet("date_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("date");
        if (type)
            type->SetText(m_currentStartTime.toString(dateformat));
    }

    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(dr.topLeft(), pix);
}

void GuideGrid::paintJumpToChannel(QPainter *p)
{
    if (!jumpToChannelEnabled || !jumpToChannelActive)
        return;

    QRect jtcr;
    LayerSet *container = NULL;

    if (jumpToChannelHasRect) {
        jtcr = jumpToChannelRect;
        container = theme->GetSet("jumptochannel");
    } else {
        jtcr = dateRect;
        container = theme->GetSet("date_info");
    }

    QPixmap pix(jtcr.size());
    pix.fill(this, jtcr.topLeft());
    QPainter tmp(&pix);

    if (container)
    {
        UITextType *type = (UITextType *)container->GetType((jumpToChannelHasRect) ? "channel" : "date");
        if (type) {
            type->SetText(QString::number(jumpToChannel));
        }
    }

    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(jtcr.topLeft(), pix);
}

void GuideGrid::paintCurrentInfo(QPainter *p)
{
    QRect dr = curInfoRect;
    QPixmap pix(dr.size());
    pix.fill(this, dr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    container = theme->GetSet("current_info");
    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(dr.topLeft(), pix);
}

void GuideGrid::paintChannels(QPainter *p)
{
    QRect cr = channelRect;
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = NULL;
    LayerSet *infocontainer = NULL;
    UIBarType *type = NULL;
    UIImageType *itype = NULL;
    container = theme->GetSet("chanbar");
    infocontainer = theme->GetSet("program_info");
    if (container)
        type = (UIBarType *)container->GetType("chans");
    if (infocontainer)
        itype = (UIImageType *)container->GetType("icon");

    if (type && type->GetNums() != DISPLAY_CHANS)
        type->SetSize(DISPLAY_CHANS);

    ChannelInfo *chinfo = &(m_channelInfos[m_currentStartChannel]);

    for (unsigned int y = 0; y < (unsigned int)DISPLAY_CHANS; y++)
    {
        unsigned int chanNumber = y + m_currentStartChannel;
        if (chanNumber >= m_channelInfos.size())
            chanNumber -= m_channelInfos.size();
        if (chanNumber >= m_channelInfos.size())
            break;  

        chinfo = &(m_channelInfos[chanNumber]);
        if ((y == (unsigned int)2 && scrolltype != 1) || 
            ((signed int)y == m_currentRow && scrolltype == 1))
        {
            if (chinfo->iconpath != "none" && chinfo->iconpath != "")
            {
                int iconsize = 0;
                if (itype)
                    iconsize = itype->GetSize();
                else if (type)
                    iconsize = type->GetSize();
                if (!chinfo->iconload)
                    chinfo->LoadIcon(iconsize);
                if (chinfo->iconload && itype)
                    itype->SetImage(chinfo->icon);
            }
        }

        QString tmpChannelFormat = channelFormat;
        if (chinfo->favid > 0)
        {
            tmpChannelFormat.insert(tmpChannelFormat.find('<'), "* ");
            tmpChannelFormat.insert(tmpChannelFormat.find('>') + 1, " *");
        }

        if (type)
        {
            if (!chinfo->iconpath.isEmpty() && chinfo->iconpath != "none")
            {
                int iconsize = 0;
                iconsize = type->GetSize();
                if (!chinfo->iconload)
                    chinfo->LoadIcon(iconsize);
                if (chinfo->iconload)
                    type->SetIcon(y, chinfo->icon);
            }
            else
            {
                type->ResetImage(y);
            }

            type->SetText(y, chinfo->Text(tmpChannelFormat));
        }
    }

    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }

    tmp.end();
    p->drawPixmap(cr.topLeft(), pix);
}

void GuideGrid::paintTimes(QPainter *p)
{
    QRect tr = timeRect;
    QPixmap pix(tr.size());
    pix.fill(this, tr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = theme->GetSet("timebar");
    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(tr.topLeft(), pix);
}

void GuideGrid::paintPrograms(QPainter *p)
{
    QRect pr = programRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    LayerSet *container = theme->GetSet("guide");
    if (container)
    {
        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }
    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void GuideGrid::paintInfo(QPainter *p)
{
    if (m_currentRow < 0 || m_currentCol < 0)
        return;

    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];
    if (!pginfo)
        return;

    QMap<QString, QString> infoMap;
    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);

    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum >= (int)m_channelInfos.size())
        return;
    if (chanNum < 0)
        chanNum = 0;

    ChannelInfo *chinfo = &(m_channelInfos[chanNum]);

    pginfo->ToMap(infoMap);

    LayerSet *container = theme->GetSet("program_info");
    if (container)
    {
        container->ClearAllText();
        container->SetText(infoMap);

        UITextType *type;

        type = (UITextType *)container->GetType("misicon");
        if (type)
            type->SetText(chinfo->callsign);

        UIImageType *itype = (UIImageType *)container->GetType("icon");
        if (itype)
        {
            itype->SetImage(chinfo->iconpath);
            itype->LoadImage();
        }

        container->Draw(&tmp, 1, m_context);
        container->Draw(&tmp, 2, m_context);
        container->Draw(&tmp, 3, m_context);
        container->Draw(&tmp, 4, m_context);
        container->Draw(&tmp, 5, m_context);
        container->Draw(&tmp, 6, m_context);
        container->Draw(&tmp, 7, m_context);
        container->Draw(&tmp, 8, m_context);
    }

    tmp.end();
    p->drawPixmap(pr.topLeft(), pix);
}

void GuideGrid::toggleGuideListing()
{
    showFavorites = (!showFavorites);
    generateListings();
}

void GuideGrid::generateListings()
{
    m_currentStartChannel = 0;
    m_currentRow = 0;

    int maxchannel = 0;
    DISPLAY_CHANS = desiredDisplayChans;
    fillChannelInfos();
    maxchannel = max((int)m_channelInfos.size() - 1, 0);
    DISPLAY_CHANS = min(DISPLAY_CHANS, maxchannel + 1);

    m_recList.FromScheduler();
    fillProgramInfos();
    update(fullRect);
}

void GuideGrid::toggleChannelFavorite()
{
    MSqlQuery query(MSqlQuery::InitCon());

    // Get current channel id, and make sure it exists...
    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum >= (int)m_channelInfos.size())
        return;
    if (chanNum < 0)
        chanNum = 0;

    int favid = m_channelInfos[chanNum].favid;
    int chanid = m_channelInfos[chanNum].chanid;

    if (favid > 0) 
    {
        query.prepare("DELETE FROM favorites WHERE favid = :FAVID ;");
        query.bindValue(":FAVID", favid);
        query.exec(); 
    }
    else
    {
        // We have no favorites record...Add one to toggle...
        query.prepare("INSERT INTO favorites (chanid) VALUES (:FAVID);");
        query.bindValue(":FAVID", chanid);
        query.exec(); 
    }

    if (showFavorites)
        generateListings();
    else
    {
        int maxchannel = 0;
        DISPLAY_CHANS = desiredDisplayChans;
        fillChannelInfos(false);
        maxchannel = max((int)m_channelInfos.size() - 1, 0);
        DISPLAY_CHANS = min(DISPLAY_CHANS, maxchannel + 1);

        repaint(channelRect, false);
    }
}

void GuideGrid::cursorLeft()
{
    ProgramInfo *test = m_programInfos[m_currentRow][m_currentCol];
    
    if (!test)
    {
        scrollLeft();
        return;
    }

    int startCol = test->startCol;
    m_currentCol = startCol - 1;

    if (m_currentCol < 0)
    {
        m_currentCol = 0;
        scrollLeft();
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        repaint(programRect, false);
        repaint(infoRect, false);
        repaint(timeRect, false);
    }
}

void GuideGrid::cursorRight()
{
    ProgramInfo *test = m_programInfos[m_currentRow][m_currentCol];

    if (!test)
    {
        scrollRight();
        return;
    }

    int spread = test->spread;
    int startCol = test->startCol;

    m_currentCol = startCol + spread;

    if (m_currentCol > DISPLAY_TIMES - 1)
    {
        m_currentCol = DISPLAY_TIMES - 1;
        scrollRight();
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        repaint(programRect, false);
        repaint(infoRect, false);
        repaint(timeRect, false); 
    }
}

void GuideGrid::cursorDown()
{
    if (scrolltype == 1)
    {
        m_currentRow++;

        if (m_currentRow > DISPLAY_CHANS - 1)
        {
            m_currentRow = DISPLAY_CHANS - 1;
            scrollDown();
        }
        else
        {
            fillProgramRowInfos(m_currentRow);
            repaint(programRect, false);
            repaint(infoRect, false);
        }
    }
    else
        scrollDown();
}

void GuideGrid::cursorUp()
{
    if (scrolltype == 1)
    {
        m_currentRow--;

        if (m_currentRow < 0)
        {
            m_currentRow = 0;
            scrollUp();
        }
        else
        {
            fillProgramRowInfos(m_currentRow);
            repaint(programRect, false);
            repaint(infoRect, false);
        }
    }
    else
        scrollUp();
}

void GuideGrid::scrollLeft()
{
    bool updatedate = false;

    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(-30 * 60);

    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    repaint(programRect, false);
    repaint(infoRect, false);
    repaint(dateRect, false);
    repaint(jumpToChannelRect, false);
    repaint(timeRect, false);
}

void GuideGrid::scrollRight()
{
    bool updatedate = false;

    QDateTime t = m_currentStartTime;
    t = m_currentStartTime.addSecs(30 * 60);

    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    repaint(programRect, false);
    repaint(infoRect, false);
    repaint(dateRect, false);
    repaint(jumpToChannelRect, false);
    repaint(timeRect, false);
}

void GuideGrid::setStartChannel(int newStartChannel)
{
    if (newStartChannel <= 0)
        m_currentStartChannel = newStartChannel + m_channelInfos.size();
    else if (newStartChannel >= (int) m_channelInfos.size())
        m_currentStartChannel = newStartChannel - m_channelInfos.size();
    else
        m_currentStartChannel = newStartChannel;
}

void GuideGrid::scrollDown()
{
    setStartChannel(m_currentStartChannel + 1);

    fillProgramInfos();

    repaint(programRect, false);
    repaint(infoRect, false);
    repaint(channelRect, false);
}

void GuideGrid::scrollUp()
{
    setStartChannel((int)(m_currentStartChannel) - 1);

    fillProgramInfos();

    repaint(programRect, false);
    repaint(infoRect, false);
    repaint(channelRect, false);
}

void GuideGrid::dayLeft()
{
    m_currentStartTime = m_currentStartTime.addSecs(-24 * 60 * 60);

    fillTimeInfos();
    fillProgramInfos();

    repaint(fullRect, false);
}

void GuideGrid::dayRight()
{
    m_currentStartTime = m_currentStartTime.addSecs(24 * 60 * 60);

    fillTimeInfos();
    fillProgramInfos();

    repaint(fullRect, false);
}

void GuideGrid::pageLeft()
{
    bool updatedate = false;

    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(-5 * 60 * DISPLAY_TIMES);

    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    repaint(fullRect, false);
}

void GuideGrid::pageRight()
{
    bool updatedate = false;

    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(5 * 60 * DISPLAY_TIMES);

    if (t.date().day() != m_currentStartTime.date().day())
        updatedate = true;

    m_currentStartTime = t;

    fillTimeInfos();
    fillProgramInfos();

    repaint(fullRect, false);
}

void GuideGrid::pageDown()
{
    setStartChannel(m_currentStartChannel + DISPLAY_CHANS);

    fillProgramInfos();

    repaint(fullRect, false);
}

void GuideGrid::pageUp()
{
    setStartChannel((int)(m_currentStartChannel) - DISPLAY_CHANS);

    fillProgramInfos();

    repaint(fullRect, false);
}
 
void GuideGrid::showProgFinder()
{
    RunProgramFind(false, true);

    setActiveWindow();
    setFocus();

    if (m_player && videoRect.height() > 0 && videoRect.width() > 0)
        m_player->EmbedOutput(this->winId(), videoRect.x(), videoRect.y(), 
                              videoRect.width(), videoRect.height());
}

void GuideGrid::enter()
{
    if (timeCheck)
    {
        timeCheck->stop();
        if (m_player)
            m_player->StopEmbeddingOutput();
    }

    unsetCursor();
    selectState = 1;
    accept();
}

void GuideGrid::escape()
{
    if (timeCheck)
    {
        timeCheck->stop();
        if (m_player)
            m_player->StopEmbeddingOutput();
    }

    unsetCursor();
    accept();
}

void GuideGrid::quickRecord()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    pginfo->ToggleRecord();

    m_recList.FromScheduler();
    fillProgramInfos();
    repaint(programRect, false);
    repaint(infoRect, false);
}

void GuideGrid::editRecording()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    FocusPolicy storeFocus = focusPolicy();
    setFocusPolicy(QWidget::NoFocus);

    ProgramInfo *temppginfo = new ProgramInfo(*pginfo);
    temppginfo->EditRecording();
    delete temppginfo;

    setFocusPolicy(storeFocus);

    setActiveWindow();
    setFocus();

    m_recList.FromScheduler();
    fillProgramInfos();
    repaint(fullRect, false);
}

void GuideGrid::editScheduled()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    FocusPolicy storeFocus = focusPolicy();
    setFocusPolicy(QWidget::NoFocus);

    ProgramInfo *temppginfo = new ProgramInfo(*pginfo);
    temppginfo->EditScheduled();
    delete temppginfo;

    setFocusPolicy(storeFocus);

    setActiveWindow();
    setFocus();

    m_recList.FromScheduler();
    fillProgramInfos();

    repaint(fullRect, false);
}

void GuideGrid::customEdit()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    CustomEdit *ce = new CustomEdit(gContext->GetMainWindow(), "customedit",
                                    pginfo->getRecordID(), pginfo->title);
    ce->exec();
    delete ce;
}

void GuideGrid::upcoming()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    ProgLister *pl = new ProgLister(plTitle, pginfo->title, "",
                                   gContext->GetMainWindow(), "proglist");
    pl->exec();
    delete pl;
}

void GuideGrid::details()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    pginfo->showDetails();
}

void GuideGrid::channelUpdate(void)
{
    if (m_channelInfos.empty())
        return;

    uint idx = (m_currentRow + m_currentStartChannel) % m_channelInfos.size();
    ChannelInfo info = m_channelInfos[idx];

    if (m_player)
        m_player->EPGChannelUpdate(info.chanid, info.chanstr);
}

//
// jumpToChannel - Jason Parekh <jasonparekh@gmail.com> - 06/23/2004
//
// The way this works is if you enter a digit, it will set a timer for 2.5 seconds and enter
// jump to channel mode where as you enter digits, it will find the closest matching channel
// and show that on the program guide.  Within the 2.5 seconds, if you press the button bound
// to END, it will cancel channel jump mode and revert to the highlighted channel before entering
// this mode.  If you let the timer expire, it will stay on the selected channel.
//
// The positioning for the channel display can be in either one of two ways.  To ensure compatibility
// with themes, I made it so if it can't find a specific jumptochannel container, it will use the date's
// and just overwrite the date whenever it is in channel jump mode.  However, if there is a jumptochannel
// container, it will use that.  An example container is as follows:
//
//     <container name="jumptochannel">
//     <area>20,20,115,25</area>
//     <textarea name="channel" draworder="4" align="right">
//         <font>info</font>
//         <area>0,0,115,25</area>
//         <cutdown>no</cutdown>
//     </textarea>
//     </container>
//

void GuideGrid::jumpToChannelDigitPress(int digit)
{
    // if this is the first digit press (coming from inactive mode)
    if (jumpToChannelActive == false) {
        jumpToChannelActive = true;
        jumpToChannel = 0;
        jumpToChannelPreviousStartChannel = m_currentStartChannel;
        jumpToChannelPreviousRow = m_currentRow;
    }
    
    // setup the timeout timer for jump mode
    jumpToChannelTimer->stop();
    jumpToChannelTimer->start(3500, true);

    jumpToChannel = (jumpToChannel * 10) + digit;

    // So it will move to the closest channels while they enter the digits
    jumpToChannelShowSelection();
}

void GuideGrid::jumpToChannelDeleteLastDigit()
{
    jumpToChannel /= 10;
    
    if (jumpToChannel == 0) 
        jumpToChannelCancel();
    else 
        jumpToChannelShowSelection();
}

void GuideGrid::jumpToChannelShowSelection()
{
    unsigned int i;
    
    // Not the best method, but the channel list is small and this isn't time critical
    // Go through until the ith channel is equal to or greater than the one we're looking for
    for (i=0; (i < m_channelInfos.size()-1) &&
             (m_channelInfos[i].chanstr.toInt() < jumpToChannel); i++);
    
    if ((i > 0) &&
        ((m_channelInfos[i].chanstr.toInt() - jumpToChannel) > (jumpToChannel - m_channelInfos[i-1].chanstr.toInt()))) {
        i--;
    }

    // DISPLAY_CHANS to center
    setStartChannel(i-DISPLAY_CHANS/2);
    m_currentRow = DISPLAY_CHANS/2;
    
    fillProgramInfos();

    repaint(fullRect, false);
}

void GuideGrid::jumpToChannelCommit()
{
    jumpToChannelResetAndHide();
}

void GuideGrid::jumpToChannelCancel()
{
    setStartChannel(jumpToChannelPreviousStartChannel);
    m_currentRow = jumpToChannelPreviousRow;
    
    fillProgramInfos();
    
    repaint(fullRect, false);
    
    jumpToChannelResetAndHide();
}

void GuideGrid::jumpToChannelResetAndHide()
{
    jumpToChannelActive = false;

    jumpToChannelTimer->stop();

    repaint(fullRect, false);
}

void GuideGrid::jumpToChannelTimeout()
{
    jumpToChannelCommit();
}

// function returns true if it is able to find a mapping to a digit and false
// otherwise.
bool GuideGrid::jumpToChannelGetInputDigit(QStringList &actions, int &digit)
{
    for (unsigned int i = 0; i < actions.size(); ++i) 
    {
        QString action = actions[i];
        if (action[0] >= '0' && action[0] <= '9') 
        {
            digit = action.toInt();
            return true;
        }
    }
    
    // There were no actions that resolved to a digit
    return false;
}
