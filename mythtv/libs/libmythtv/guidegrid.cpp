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
#include "guidegrid.h"
#include "infodialog.h"
#include "infostructs.h"
#include "programinfo.h"
#include "oldsettings.h"
#include "tv.h"
#include "progfind.h"
#include "util.h"

QString RunProgramGuide(QString startchannel, bool thread, TV *player)
{
    QString chanstr;
   
    if (thread)
        qApp->lock();

    if (startchannel == QString::null)
        startchannel = "";

    GuideGrid *gg = new GuideGrid(gContext->GetMainWindow(), startchannel, 
                                  player, "guidegrid");

    gg->Show();

    if (thread)
    {
        qApp->unlock();

        while (gg->isVisible())
            usleep(50);
    }
    else
        gg->exec();

    chanstr = gg->getLastChannel();

    if (thread)
        qApp->lock();

    delete gg;

    if (thread)
        qApp->unlock();

    if (chanstr == QString::null)
        chanstr = "";

    return chanstr;
}

GuideGrid::GuideGrid(MythMainWindow *parent, const QString &channel, TV *player,
                     const char *name)
         : MythDialog(parent, name)
{
    desiredDisplayChans = DISPLAY_CHANS = 6;
    DISPLAY_TIMES = 30;
    int maxchannel = 0;
    m_currentStartChannel = 0;

    m_player = player;
    m_db = QSqlDatabase::database();

    m_context = 0;

    fullRect = QRect(0, 0, size().width(), size().height());
    dateRect = QRect(0, 0, 0, 0);
    channelRect = QRect(0, 0, 0, 0);
    timeRect = QRect(0, 0, 0, 0);
    programRect = QRect(0, 0, 0, 0);
    infoRect = QRect(0, 0, 0, 0);
    curInfoRect = QRect(0, 0, 0, 0);
    videoRect = QRect(0, 0, 0, 0);

    MythContext::KickDatabase(m_db);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (m_player && m_player->IsRunning())
    {
        theme->LoadTheme(xmldata, "programguide-video");
    }
    else 
    {
        theme->LoadTheme(xmldata, "programguide");
    }
    LoadWindow(xmldata);

    showFavorites = gContext->GetNumSetting("EPGShowFavorites", 0);
    gridfilltype = gContext->GetNumSetting("EPGFillType", 6);
    scrolltype = gContext->GetNumSetting("EPGScrollType", 1);

    LayerSet *container = NULL;
    container = theme->GetSet("guide");
    if (container)
    {
        UIGuideType *type = (UIGuideType *)container->GetType("guidegrid");
        if (type)
            type->SetFillType(gridfilltype);
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
    }

    channelOrdering = gContext->GetSetting("ChannelOrdering", "channum + 0");
    dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    unknownTitle = gContext->GetSetting("UnknownTitle", "Unknown");
    unknownCategory = gContext->GetSetting("UnknownCategory", "Unknown");
    currentTimeColor = gContext->GetSetting("EPGCurrentTimeColor", "red");
    displaychannum = gContext->GetNumSetting("DisplayChanNum");

    UIBarType *type = NULL;
    container = theme->GetSet("chanbar");

    int dNum = gContext->GetNumSetting("chanPerPage", 8);
    desiredDisplayChans = DISPLAY_CHANS = dNum;
    if (container)
    {
        type = (UIBarType *)container->GetType("chans");
        if (type)
            type->SetSize(dNum);
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
    }
    m_originalStartTime = QDateTime::currentDateTime();

    int secsoffset = -((m_originalStartTime.time().minute() % 30) * 60 +
                        m_originalStartTime.time().second());
    m_currentStartTime = m_originalStartTime.addSecs(secsoffset);
    m_startChanStr = channel;

    m_currentRow = (int)(desiredDisplayChans / 2);
    m_currentCol = 0;

    for (int y = 0; y < DISPLAY_CHANS; y++)
        m_programs[y] = NULL;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        m_timeInfos[x] = NULL;
        for (int y = 0; y < DISPLAY_CHANS; y++)
            m_programInfos[y][x] = NULL;
    }

    //QTime clock = QTime::currentTime();
    //clock.start();
    fillTimeInfos();
    //int filltime = clock.elapsed();
    //clock.restart();
    fillChannelInfos(maxchannel);
    setStartChannel((int)(m_currentStartChannel) - 
                    (int)(desiredDisplayChans / 2));
    if (DISPLAY_CHANS > maxchannel)
        DISPLAY_CHANS = maxchannel;

    //int fillchannels = clock.elapsed();
    //clock.restart();
    fillProgramInfos();
    //int fillprogs = clock.elapsed();

    timeCheck = NULL;
    timeCheck = new QTimer(this);
    connect(timeCheck, SIGNAL(timeout()), SLOT(timeout()) );
    timeCheck->start(200);

    selectState = false;
    showInfo = false;

    updateBackground();

    setNoErase();
}

GuideGrid::~GuideGrid()
{
    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x])
            delete m_timeInfos[x];
    }

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        if (m_programs[y])
            delete m_programs[y];
    }

    m_channelInfos.clear();

    delete theme;
}

void GuideGrid::keyPressEvent(QKeyEvent *e)
{
    if (e->state() == Qt::ControlButton)
    {
        switch (e->key())
        {
            case Key_Left: pageLeft(); return;
            case Key_Right: pageRight(); return;
            case Key_Up: pageUp(); return;
            case Key_Down: pageDown(); return;
            default: break;
        }
    }

    switch (e->key())
    {
        case Key_Left: case Key_A: cursorLeft(); break;
        case Key_Right: case Key_D: cursorRight(); break;
        case Key_Down: case Key_S: cursorDown(); break;
        case Key_Up: case Key_W: cursorUp(); break;

        case Key_Home: case Key_7: dayLeft(); break;
        case Key_End: case Key_1: dayRight(); break;
        case Key_PageUp: case Key_3: pageUp(); break;
        case Key_PageDown: case Key_9: pageDown(); break;
      
        case Key_4: toggleGuideListing(); break;
        case Key_6: showProgFinder(); break;   
      
        case Key_Slash: toggleChannelFavorite(); break;
 
        case Key_C: case Key_Escape: escape(); break;
        case Key_M: enter(); break;

        case Key_I: case Key_Space: 
        case Key_Enter: case Key_Return:  displayInfo(); break;

        case Key_R: quickRecord(); break;
        case Key_X: channelUpdate(); break;

        default: MythDialog::keyPressEvent(e);
    }
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
                exit(0);
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
    if (name.lower() == "current_info")
        curInfoRect = area;
    if (name.lower() == "current_video")
        videoRect = area;
}

QString GuideGrid::getDateLabel(ProgramInfo *pginfo)
{
    QDateTime startts = pginfo->startts;
    QDateTime endts = pginfo->endts;

    QString timedate = startts.date().toString(dateformat) + QString(", ") +
                       startts.time().toString(timeformat) + QString(" - ") +
                       endts.time().toString(timeformat);

    return timedate;
}

QString GuideGrid::getLastChannel(void)
{
    unsigned int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= m_channelInfos.size())
        chanNum -= m_channelInfos.size();

    if (selectState)
        return m_channelInfos[chanNum].chanstr;
    return 0;
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

    update(curInfoRect);
}

void GuideGrid::fillChannelInfos(int &maxchannel, bool gotostartchannel)
{
    m_channelInfos.clear();

    QString queryfav;
    QSqlQuery query;

    QString queryall = "SELECT channel.channum, channel.callsign, "
                       "channel.icon, channel.chanid, favorites.favid "
                       "FROM channel LEFT JOIN favorites ON "
                       "favorites.chanid = channel.chanid ORDER BY " +
                       channelOrdering + ";";

    if (showFavorites)
    {
        queryfav = "SELECT channel.channum, channel.callsign, "
                   "channel.icon, channel.chanid, favorites.favid "
                   "FROM favorites, channel WHERE "
                   "channel.chanid = favorites.chanid ORDER BY " + 
                   channelOrdering + ";";

        query.exec(queryfav);   

        // If we don't have any favorites, then just show regular listings.
        if (!query.isActive() || query.numRowsAffected() == 0)
        {
            showFavorites = (!showFavorites);
            query.exec(queryall);
        }
    }
    else
    {
        query.exec(queryall);
    }
 
    bool set = false;
    maxchannel = 0;
    
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ChannelInfo val;
            val.chanstr = query.value(0).toString();
            if ((val.chanstr != QString::null) && (val.chanstr != ""))
            {
                val.callsign = query.value(1).toString();
                if (val.callsign == QString::null)
                    val.callsign = "";
                val.iconpath = query.value(2).toString();
                if (val.iconpath == QString::null)
                    val.iconpath = "none";
                if (val.iconpath.stripWhiteSpace().length() == 0)
                    val.iconpath = "none";
                val.chanstr = query.value(0).toString();
                if (val.chanstr == QString::null)
                val.chanstr = "";
                val.chanid = query.value(3).toInt();
                val.favid = query.value(4).toInt();
                val.icon = NULL;
        
                if (gotostartchannel && val.chanstr == m_startChanStr && !set)
                {
                    m_currentStartChannel = m_channelInfos.size();
                    set = true;
                }
                
                m_channelInfos.push_back(val);
                maxchannel++;
            }
        }
    }
    else
    {
        cerr << "You don't have any channels defined in the database\n";
        cerr << "This isn't going to work.\n";
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
            type->ResetData();
            type->SetWindow(this);
            type->SetScreenLocation(programRect.topLeft());
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
 
    QPtrList<ProgramInfo> *proglist;
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
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum < 0)
        chanNum = 0;

    m_programs[row] = proglist = new QPtrList<ProgramInfo>;
    m_programs[row]->setAutoDelete(true);

    QString chanid = QString("%1").arg(m_channelInfos[chanNum].chanid);

    char temp[16];
    sprintf(temp, "%4d%02d%02d%02d%02d00",
            m_currentStartTime.date().year(),
            m_currentStartTime.date().month(),
            m_currentStartTime.date().day(),
            m_currentStartTime.time().hour(),
            m_currentStartTime.time().minute());
    QString starttime = temp;
    sprintf(temp, "%4d%02d%02d%02d%02d00",
            m_currentEndTime.date().year(),
            m_currentEndTime.date().month(),
            m_currentEndTime.date().day(),
            m_currentEndTime.time().hour(),
            m_currentEndTime.time().minute());
    QString endtime = temp;

    ProgramInfo::GetProgramRangeDateTime(m_db, proglist, chanid, starttime, 
                                         endtime);

    QDateTime ts = m_currentStartTime;

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
    int recFlag = 0;
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
                recFlag = 0;
                RecordingType recordtype;
                recordtype = proginfo->GetProgramRecordingStatus(m_db);
                if (recordtype > kNotRecording)
                {
                    switch (recordtype) 
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
                        case kNotRecording:
                            break;
                    }
                }

                type->SetProgramInfo(row + 1, cnt, tempRect, proginfo->title,
                                     proginfo->category, arrow, recFlag);
                if (isCurrent == true)
                { 
                    type->SetCurrentArea(tempRect);
                }

                cnt++;
            }
        }

        lastprog = proginfo->startts;
    } 
}

void GuideGrid::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (!showInfo)
    {
        if (r.intersects(infoRect))
            paintInfo(&p);
        if (r.intersects(dateRect))
            paintDate(&p);
        if (r.intersects(channelRect))
            paintChannels(&p);
        if (r.intersects(timeRect))
            paintTimes(&p);
        if (r.intersects(programRect))
            paintPrograms(&p);
        if (r.intersects(curInfoRect))
            paintCurrentInfo(&p);
    }
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
            type->SetText(m_currentStartTime.toString("ddd"));
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

    ChannelInfo *chinfo = &(m_channelInfos[m_currentStartChannel]);

    for (unsigned int y = 0; y < (unsigned int)DISPLAY_CHANS; y++)
    {
        unsigned int chanNumber = y + m_currentStartChannel;
        if (chanNumber >= m_channelInfos.size())
            chanNumber -= m_channelInfos.size();
  
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
                if (!chinfo->icon)
                    chinfo->LoadIcon(iconsize);
                if (chinfo->icon && itype)
                    itype->SetImage(*(chinfo->icon));
            }
        }

        QString favstr = "";
        if (chinfo->favid > 0)
            favstr = "*";

        QString write = "";
        QString callsignstr = favstr + " " + chinfo->callsign + " " +
                              favstr;

        if (type)
        {
            if (chinfo->iconpath != "none" && chinfo->iconpath != "")
            {
                int iconsize = 0;
                iconsize = type->GetSize();
                if (!chinfo->icon)
                    chinfo->LoadIcon(iconsize);
                if (chinfo->icon)
                    type->SetIcon(y, *(chinfo->icon));
            }
            else
            {
                type->ResetImage(y);
            }

            if (!displaychannum)
                write = chinfo->chanstr + "\n";
        
            write = write + callsignstr;
            type->SetText(y, write);
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
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];
    if (!pginfo)
        return;

    QRect pr = infoRect;
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());
    QPainter tmp(&pix);
    QString recStatus = "";

    RecordingType recordtype;
    recordtype = pginfo->GetProgramRecordingStatus(m_db);
    switch (recordtype) 
    {
        case kSingleRecord:
            recStatus = tr("Recording Once");
            break;
        case kTimeslotRecord:
            recStatus = tr("Timeslot Recording");
            break;
        case kWeekslotRecord:
            recStatus = tr("Weekly Recording");
            break;
        case kChannelRecord:
            recStatus = tr("Channel Recording");
            break;
        case kAllRecord:
            recStatus = tr("All Recording");
            break;
        case kNotRecording:
            recStatus = tr("Not Recording");
            break;
    }

    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum < 0)
        chanNum = 0;

    ChannelInfo *chinfo = &(m_channelInfos[chanNum]);

    LayerSet *container = theme->GetSet("program_info");
    if (container)
    {
        UITextType *type = (UITextType *)container->GetType("title");
        if (type)
            type->SetText(pginfo->title);
        type = (UITextType *)container->GetType("misicon");
        if (type)
            type->SetText(chinfo->callsign);

        if ((pginfo->subtitle).stripWhiteSpace().length() != 0)
        {
           type = (UITextType *)container->GetType("subtitle");
           if (type)
               type->SetText("\"" + pginfo->subtitle + "\"");
        }  
        else
        {
           type = (UITextType *)container->GetType("subtitle");
           if (type)
               type->SetText("");
    
        }
        type = (UITextType *)container->GetType("timedate");
        if (type)
            type->SetText(getDateLabel(pginfo));
        type = (UITextType *)container->GetType("description");
        if (type)
            type->SetText(pginfo->description);
        type = (UITextType *)container->GetType("recordingstatus");
        if (type)
            type->SetText(recStatus);
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
    fillChannelInfos(maxchannel);
    if (DISPLAY_CHANS > maxchannel)
        DISPLAY_CHANS = maxchannel;

    fillProgramInfos();
    update(fullRect);
}

void GuideGrid::toggleChannelFavorite()
{
    QString thequery;
    QSqlQuery query;

    // Get current channel id, and make sure it exists...
    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum < 0)
        chanNum = 0;

    int favid = m_channelInfos[chanNum].favid;
    int chanid = m_channelInfos[chanNum].chanid;

    if (favid > 0) 
    {
        thequery = QString("DELETE FROM favorites WHERE favid = '%1'")
                           .arg(favid);

        query.exec(thequery);
    }
    else
    {
        // We have no favorites record...Add one to toggle...
        thequery = QString("INSERT INTO favorites (chanid) VALUES ('%1')")
                           .arg(chanid);

        query.exec(thequery);
    }

    if (showFavorites)
        generateListings();
    else
    {
        int maxchannel = 0;
        DISPLAY_CHANS = desiredDisplayChans;
        fillChannelInfos(maxchannel, false);
        if (DISPLAY_CHANS > maxchannel)
            DISPLAY_CHANS = maxchannel;

        update(channelRect);
    }
}

void GuideGrid::cursorLeft()
{
    ProgramInfo *test = m_programInfos[m_currentRow][m_currentCol];
    
    if (!test)
    {
        emit scrollLeft();
        return;
    }

    int startCol = test->startCol;
    m_currentCol = startCol - 1;

    if (m_currentCol < 0)
    {
        m_currentCol = 0;
        emit scrollLeft();
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        update(programRect);
        update(infoRect);
        update(timeRect);
    }
}

void GuideGrid::cursorRight()
{
    ProgramInfo *test = m_programInfos[m_currentRow][m_currentCol];

    if (!test)
    {
        emit scrollRight();
        return;
    }

    int spread = test->spread;
    int startCol = test->startCol;

    m_currentCol = startCol + spread;

    if (m_currentCol > DISPLAY_TIMES - 1)
    {
        m_currentCol = DISPLAY_TIMES - 1;
        emit scrollRight();
    }
    else
    {
        fillProgramRowInfos(m_currentRow);
        update(programRect);
        update(infoRect);
        update(timeRect);
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
            emit scrollDown();
        }
        else
        {
            fillProgramRowInfos(m_currentRow);
            update(programRect);
            update(infoRect);
        }
    }
    else
        emit scrollDown();
}

void GuideGrid::cursorUp()
{
    if (scrolltype == 1)
    {
        m_currentRow--;

        if (m_currentRow < 0)
        {
            m_currentRow = 0;
            emit scrollUp();
        }
        else
        {
            fillProgramRowInfos(m_currentRow);
            update(programRect);
            update(infoRect);
        }
    }
    else
        emit scrollUp();
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

    update(infoRect);
    update(timeRect);
    update(dateRect);
    update(programRect);
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

    update(infoRect);
    update(dateRect);
    update(timeRect);
    update(programRect);
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

    QPtrList<ProgramInfo> *proglist = m_programs[0];
    for (int y = 0; y < DISPLAY_CHANS - 1; y++)
    {
        m_programs[y] = m_programs[y + 1];
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            m_programInfos[y][x] = m_programInfos[y + 1][x];
        }
    } 
    m_programs[DISPLAY_CHANS - 1] = proglist;
    fillProgramInfos();

    update(infoRect);
    update(channelRect);
    update(programRect);
}

void GuideGrid::scrollUp()
{
    setStartChannel((int)(m_currentStartChannel) - 1);

    QPtrList<ProgramInfo> *proglist = m_programs[DISPLAY_CHANS - 1];
    for (int y = DISPLAY_CHANS - 1; y > 0; y--)
    {
        m_programs[y] = m_programs[y - 1];
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            m_programInfos[y][x] = m_programInfos[y - 1][x];
        }
    } 
    m_programs[0] = proglist;
    fillProgramInfos();

    update(infoRect);
    update(channelRect);
    update(programRect);
}

void GuideGrid::dayLeft()
{
    m_currentStartTime = m_currentStartTime.addSecs(-24 * 60 * 60);

    fillTimeInfos();
    fillProgramInfos();

    update(fullRect);
}

void GuideGrid::dayRight()
{
    m_currentStartTime = m_currentStartTime.addSecs(24 * 60 * 60);

    fillTimeInfos();
    fillProgramInfos();

    update(fullRect);
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

    update(fullRect);
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

    update(fullRect);
}

void GuideGrid::pageDown()
{
    setStartChannel(m_currentStartChannel + DISPLAY_CHANS);

    fillProgramInfos();

    update(fullRect);
}

void GuideGrid::pageUp()
{
    setStartChannel((int)(m_currentStartChannel) - DISPLAY_CHANS);

    fillProgramInfos();

    update(fullRect);
}
 
void GuideGrid::showProgFinder()
{
    showInfo = 1;

    RunProgramFind();

    showInfo = 0;

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
    emit accept();
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
    emit accept();
}

void GuideGrid::quickRecord()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    pginfo->ToggleRecord(m_db);

    fillProgramInfos();
    update(programRect);
    update(infoRect);
}

void GuideGrid::displayInfo()
{
    showInfo = 1;

    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (!pginfo)
        return;

    if (pginfo->title == unknownTitle)
        return;

    if (pginfo)
    {
        if ((gContext->GetNumSetting("AdvancedRecord", 0)) ||
            (pginfo->GetProgramRecordingStatus(m_db) > kAllRecord))
        {
            ScheduledRecording record;
            record.loadByProgram(m_db, pginfo);
            record.exec(m_db);
        }
        else
        {
            InfoDialog diag(pginfo, gContext->GetMainWindow(), "Program Info");
            diag.exec();
        }
    }
    else
        return;

    showInfo = 0;

    pginfo->GetProgramRecordingStatus(m_db);

    setActiveWindow();
    setFocus();

    fillProgramInfos();
    update(fullRect);
}

void GuideGrid::channelUpdate(void)
{
    if (!m_player)
        return;

    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum < 0)
        chanNum = 0;

    m_player->EPGChannelUpdate(m_channelInfos[chanNum].chanstr);
}

