#include <qapplication.h>
#include <qpainter.h>
#include <qfont.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qaccel.h>
#include <math.h>
#include <qcursor.h>
#include <qapplication.h>
#include <qimage.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qdatetime.h>
#include <qvgroupbox.h>
#include <qheader.h>

#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "guidegrid.h"
#include "infodialog.h"
#include "infostructs.h"
#include "programinfo.h"
#include "oldsettings.h"

using namespace libmyth;

GuideGrid::GuideGrid(MythContext *context, const QString &channel, 
                     void (*embedcb)(void *data, unsigned long wid, int x, 
                                     int y, int w, int h), void *data,
                     QWidget *parent, const char *name)
         : MythDialog(context, parent, name)
{
    DISPLAY_CHANS = 6;

    embedcallback = embedcb;
    callbackdata = data;
    m_settings = context->settings();

    usetheme = m_settings->GetNumSetting("ThemeQt");
    showtitle = m_settings->GetNumSetting("EPGShowTitle");
    showIcon = m_settings->GetNumSetting("EPGShowChannelIcon");

    bgcolor = paletteBackgroundColor();
    fgcolor = paletteForegroundColor();

    programGuideType = m_settings->GetNumSetting("EPGType");
    if (programGuideType == 1)
    {
        DISPLAY_CHANS = 8;
        setupColorScheme();
    }

    int timefontsize = m_settings->GetNumSetting("EPGTimeFontSize");
    if (timefontsize == 0) 
        timefontsize = 13;
    m_timeFont = new QFont("Arial", (int)(timefontsize * hmult), QFont::Bold);

    int chanfontsize = m_settings->GetNumSetting("EPGChanFontSize");
    if (chanfontsize == 0) 
        chanfontsize = 13;
    m_chanFont = new QFont("Arial", (int)(chanfontsize * hmult), QFont::Bold);

    chanfontsize = m_settings->GetNumSetting("EPGChanCallsignFontSize");
    if (chanfontsize == 0)
        chanfontsize = 11;
    m_chanCallsignFont = new QFont("Arial", (int)(chanfontsize * hmult), 
                                   QFont::Bold);

    int progfontsize = m_settings->GetNumSetting("EPGProgFontSize");
    if (progfontsize == 0) 
        progfontsize = 13;
    m_progFont = new QFont("Arial", (int)(progfontsize * hmult), QFont::Bold);

    int titlefontsize = m_settings->GetNumSetting("EPGTitleFontSize");
    if (titlefontsize == 0) 
        titlefontsize = 19;
    m_titleFont = new QFont("Arial", (int)(titlefontsize * hmult), QFont::Bold);

    m_originalStartTime = QDateTime::currentDateTime();

    int secsoffset = -((m_originalStartTime.time().minute() % 30) * 60 +
                        m_originalStartTime.time().second());
    m_currentStartTime = m_originalStartTime.addSecs(secsoffset);
    m_currentStartChannel = 0;
    m_startChanStr = channel;

    m_currentRow = 0;
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
    fillChannelInfos();
    //int fillchannels = clock.elapsed();
    //clock.restart();
    fillProgramInfos();
    //int fillprogs = clock.elapsed();

    //cout << filltime << " " << fillchannels << " " << fillprogs << endl;

    if (programGuideType == 1)
        createProgramLabel(titlefontsize, progfontsize);

    QAccel *accel = new QAccel(this);
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(cursorUp()));

    accel->connectItem(accel->insertItem(Key_A), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_D), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_S), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_W), this, SLOT(cursorUp()));

    accel->connectItem(accel->insertItem(Key_Home), this, SLOT(dayLeft()));
    accel->connectItem(accel->insertItem(Key_End), this, SLOT(dayRight()));
    accel->connectItem(accel->insertItem(CTRL + Key_Left), this, 
                       SLOT(pageLeft()));
    accel->connectItem(accel->insertItem(CTRL + Key_Right), this, 
                       SLOT(pageRight()));
    accel->connectItem(accel->insertItem(Key_PageUp), this, SLOT(pageUp()));
    accel->connectItem(accel->insertItem(Key_PageDown), this, SLOT(pageDown()));
    accel->connectItem(accel->insertItem(CTRL + Key_Up), this,
                       SLOT(pageUp()));
    accel->connectItem(accel->insertItem(CTRL + Key_Down), this,
                       SLOT(pageDown()));

    accel->connectItem(accel->insertItem(Key_7), this, SLOT(dayLeft()));
    accel->connectItem(accel->insertItem(Key_1), this, SLOT(dayRight()));
    accel->connectItem(accel->insertItem(Key_9), this, SLOT(pageUp()));
    accel->connectItem(accel->insertItem(Key_3), this, SLOT(pageDown()));

    accel->connectItem(accel->insertItem(Key_C), this, SLOT(escape()));
    accel->connectItem(accel->insertItem(Key_Escape), this, SLOT(escape()));
    accel->connectItem(accel->insertItem(Key_M), this, SLOT(enter()));

    accel->connectItem(accel->insertItem(Key_I), this, SLOT(displayInfo()));
    accel->connectItem(accel->insertItem(Key_Space), this, SLOT(displayInfo()));
    accel->connectItem(accel->insertItem(Key_Enter), this, SLOT(displayInfo()));

    connect(this, SIGNAL(killTheApp()), this, SLOT(accept()));

    if (programGuideType == 1)
    {
        // One second timer for clock
        QTimer *timeCheck = new QTimer(this);
        connect(timeCheck, SIGNAL(timeout()), SLOT(timeout()) );
        timeCheck->start(1000);
    }

    selectState = false;
    showInfo = false;

    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
    setWFlags(flags);

    showFullScreen();
    setActiveWindow();
    raise();
    setFocus();
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

    delete m_timeFont;
    delete m_chanFont;
    delete m_chanCallsignFont;
    delete m_progFont;
    delete m_titleFont;
}

void GuideGrid::setupColorScheme()
{

    Settings *themed = m_context->qtconfig();
    QString curColor = "";
    curColor = themed->GetSetting("curTimeChan_bgColor");
    if (curColor != "")
        curTimeChan_bgColor = QColor(curColor);

    curColor = themed->GetSetting("curTimeChan_fgColor");
    if (curColor != "")
        curTimeChan_fgColor = QColor(curColor);

    curColor = themed->GetSetting("date_bgColor");
    if (curColor != "")
        date_bgColor = QColor(curColor);

    curColor = themed->GetSetting("date_dsColor");
    if (curColor != "")
        date_dsColor = QColor(curColor);

    curColor = themed->GetSetting("date_fgColor");
    if (curColor != "")
        date_fgColor = QColor(curColor);

    curColor = themed->GetSetting("chan_bgColor");
    if (curColor != "")
        chan_bgColor = QColor(curColor);

    curColor = themed->GetSetting("chan_dsColor");
    if (curColor != "")
        chan_dsColor = QColor(curColor);

    curColor = themed->GetSetting("chan_fgColor");
    if (curColor != "")
        chan_fgColor = QColor(curColor);

    curColor = themed->GetSetting("time_bgColor");
    if (curColor != "")
        time_bgColor = QColor(curColor);

    curColor = themed->GetSetting("time_dsColor");
    if (curColor != "")
        time_dsColor = QColor(curColor);

    curColor = themed->GetSetting("time_fgColor");
    if (curColor != "")
        time_fgColor = QColor(curColor);

        curColor = themed->GetSetting("prog_bgColor");
    if (curColor != "")
        prog_bgColor = QColor(curColor);

    curColor = themed->GetSetting("prog_fgColor");
    if (curColor != "")
        prog_fgColor = QColor(curColor);

    curColor = themed->GetSetting("progLine_Color");
    if (curColor != "")
        progLine_Color = QColor(curColor);

    curColor = themed->GetSetting("progArrow_Color");
    if (curColor != "")
        progArrow_Color = QColor(curColor);

    curColor = themed->GetSetting("curProg_bgColor");
    if (curColor != "")
        curProg_bgColor = QColor(curColor);

    curColor = themed->GetSetting("curRecProg_bgColor");
    if (curColor != "")
        curRecProg_bgColor = QColor(curColor);

    curColor = themed->GetSetting("curProg_dsColor");
    if (curColor != "")
        curProg_dsColor = QColor(curColor);

    curColor = themed->GetSetting("curProg_fgColor");
    if (curColor != "")
        curProg_fgColor = QColor(curColor);

    curColor = themed->GetSetting("misChanIcon_bgColor");
    if (curColor != "")
        misChanIcon_bgColor = QColor(curColor);

    curColor = themed->GetSetting("misChanIcon_fgColor");
    if (curColor != "")
        misChanIcon_fgColor = QColor(curColor);

    progArrow_Type = themed->GetNumSetting("progArrow_Type");
}

void GuideGrid::createProgramLabel(int titlefontsize, int progfontsize)
{
    QBoxLayout *mainHold = new QVBoxLayout( this );
    mainHold->setResizeMode(QLayout::Minimum);
    mainHold->addStrut((int)(800*wmult));

    QHBoxLayout *holdA = new QHBoxLayout(0, 0, 0);
    QHBoxLayout *holdB = new QHBoxLayout(0, 0, 0);

    holdA->addStrut((int)(300*hmult));
    mainHold->addLayout(holdA, 0);
    mainHold->addLayout(holdB, 0);

    QVBoxLayout *holdC = new QVBoxLayout(0, (int)(10 * wmult), -1);
    QVBoxLayout *holdD = new QVBoxLayout(0, 0, 0);

    holdA->addLayout(holdC, 0);
    holdA->addLayout(holdD, 0);

    QHBoxLayout *titleHold = new QHBoxLayout(0, 0, 0);

    channelimage = new QLabel("    ", this);
    channelimage->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    channelimage->setMaximumWidth((int)(50*wmult));
    channelimage->setMinimumWidth((int)(30*wmult));
    channelimage->setMinimumHeight((int)(30*hmult));
    channelimage->setPaletteForegroundColor(misChanIcon_fgColor);
    titlefield = new QLabel("", this);
    titlefield->setMaximumWidth((int)(440 * wmult) - (int)(75*wmult));
    titlefield->setMinimumWidth((int)(440 * wmult) - (int)(75*wmult));
    titlefield->setMaximumHeight((int)(75*hmult));
    titlefield->setBackgroundOrigin(WindowOrigin);
    titlefield->setFont(QFont("Arial", (int)(titlefontsize * hmult), 
                        QFont::Bold));
    titleHold->addWidget(channelimage, 0, Qt::AlignHCenter | Qt::AlignVCenter );
    titleHold->addWidget(titlefield, 0, 0);

    setFont(QFont("Arial", (int)(progfontsize * hmult), QFont::Bold));

    QPixmap temp((int)(360 * wmult), (int)(267 * hmult));
    temp.fill(black);

    forvideo = new QLabel("", this);
    forvideo->setMinimumHeight((int)(267*hmult));
    forvideo->setMaximumHeight((int)(267*hmult));
    forvideo->setMinimumWidth((int)(360*wmult));
    forvideo->setMaximumWidth((int)(360*wmult));
    forvideo->setPixmap(temp);
    QHBoxLayout *curInfo = new QHBoxLayout(0, 0, 0);

    QLabel *filler = new QLabel("", this);
    filler->setMinimumHeight((int)(3*hmult));
    filler->setMaximumHeight((int)(3*hmult));
    filler->setBackgroundOrigin(WindowOrigin);

    holdD->addWidget(forvideo, 0, 0);
    holdD->addLayout(curInfo, 0);
    holdD->addWidget(filler, 0, 0);

    currentTime = new QLabel("", this);
    currentTime->setMaximumHeight((int)(30*hmult));
    currentTime->setMinimumWidth((int)(180*wmult));
    // CURRENT TIME AND CHANNEL BACKGROUND COLOR
    currentTime->setPaletteBackgroundColor(curTimeChan_bgColor);
    // CURRENT TIME AND CHANNEL FOREGROUND COLOR
    currentTime->setPaletteForegroundColor(curTimeChan_fgColor);

    ChannelInfo *chinfo = &(m_channelInfos[m_currentStartChannel]);
    currentChan = new QLabel(chinfo->chanstr + "*" + chinfo->callsign, this);
    currentChan->setMinimumWidth((int)(180*wmult));
    currentChan->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // CURRENT TIME AND CHANNEL BACKGROUND COLOR
    currentChan->setPaletteBackgroundColor(curTimeChan_bgColor);
    // CURRENT TIME AND CHANNEL FOREGROUND COLOR
    currentChan->setPaletteForegroundColor(curTimeChan_fgColor);

    curInfo->addWidget(currentTime, 0, 0);
    curInfo->addWidget(currentChan, 0, 0);

    date = new QLabel("", this);
    date->setMaximumHeight((int)(30*hmult));
    date->setBackgroundOrigin(WindowOrigin);
    QVBoxLayout *holdCG = new QVBoxLayout(0, 0, 0);

    holdC->addLayout(titleHold, 0);
    holdC->addWidget(date, 0, 0);
    holdC->addLayout(holdCG, 0);

    QGridLayout *holdCGA = new QGridLayout(0, 3, 2, 0, -1, "");
    descriptionfield = new QLabel("", this);
    descriptionfield->setBackgroundOrigin(WindowOrigin);
    descriptionfield->setAlignment(Qt::WordBreak | Qt::AlignLeft |
                                   Qt::AlignTop);

    holdCG->addLayout(holdCGA, 0);
    holdCG->addWidget(descriptionfield, 0, 0);

    QLabel *subtitlelabel = new QLabel("Episode:", this);
    QLabel *recordinglabel = new QLabel("Recording:", this);
    QLabel *descriptionlabel = new QLabel("Description:", this);
    descriptionlabel->setMaximumWidth((int)(125*wmult));
    descriptionlabel->setMaximumHeight((int)(20*hmult));
    descriptionlabel->setBackgroundOrigin(WindowOrigin);
    subtitlelabel->setBackgroundOrigin(WindowOrigin);
    subtitlelabel->setMaximumHeight((int)(20*hmult));
    subtitlefield = new QLabel("", this);
    subtitlefield->setBackgroundOrigin(WindowOrigin);
    subtitlefield->setMaximumWidth((int)(440 * wmult) -
                                   subtitlelabel->width() - (int)(50*wmult));
    recordinglabel->setMaximumHeight((int)(20*hmult));
    recordinglabel->setBackgroundOrigin(WindowOrigin);


    recordingfield = new QLabel("", this);
    recordingfield->setBackgroundOrigin(WindowOrigin);
    QLabel *blankfield = new QLabel("", this);
    blankfield->setBackgroundOrigin(WindowOrigin);

    holdCGA->addWidget(subtitlelabel, 0, 0, 0);
    holdCGA->addWidget(subtitlefield, 0, 1, 0);
    holdCGA->addWidget(recordinglabel, 1, 0, 0);
    holdCGA->addWidget(recordingfield, 1, 1, 0);
    holdCGA->addWidget(descriptionlabel, 2, 0, 0);
    holdCGA->addWidget(blankfield, 2, 1, 0);
}

QString GuideGrid::getDateLabel(ProgramInfo *pginfo)
{
    QDateTime startts = pginfo->startts;
    QDateTime endts = pginfo->endts;

    QString dateformat = m_context->settings()->GetSetting("DateFormat");
    if (dateformat == "")
        dateformat = "ddd MMMM d";
    QString timeformat = m_context->settings()->GetSetting("TimeFormat");
    if (timeformat == "")
        timeformat = "h:mm AP";

    QString timedate = endts.date().toString(dateformat) + QString(", ") +
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
    QTime new_time = QTime::currentTime();
    QString curTime = new_time.toString("h:mm:ss ap");
    currentTime->setText("  " + curTime);

    if (embedcallback)
        embedcallback(callbackdata, forvideo->winId(), 0, 0, 
                      forvideo->width(), forvideo->height());
}

void GuideGrid::fillChannelInfos()
{
    m_channelInfos.clear();

    QString thequery;
    QSqlQuery query;

    QString ordering = m_settings->GetSetting("ChannelSorting");
    if (ordering == "")
        ordering = "channum + 0";
    
    thequery = "SELECT channum,callsign,icon,chanid FROM channel "
               "ORDER BY " + ordering;
    query.exec(thequery);
    
    bool set = false;
    
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            ChannelInfo val;
            val.callsign = query.value(1).toString();
            if (val.callsign == QString::null)
                val.callsign = "";
            val.iconpath = query.value(2).toString();
            if (val.iconpath == QString::null)
                val.iconpath = "";
            val.chanstr = query.value(0).toString();
            if (val.chanstr == QString::null)
                val.chanstr = "";
            val.chanid = query.value(3).toInt();
            val.icon = NULL;
        
            if (val.chanstr == m_startChanStr && !set)
            {
                m_currentStartChannel = m_channelInfos.size();
                set = true;
            }
		
            m_channelInfos.push_back(val);
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

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        TimeInfo *timeinfo = new TimeInfo;

        int mins = t.time().minute();
        mins = 5 * (mins / 5);
        if (mins % 30 == 0)
        {
            int hour = t.time().hour();
            timeinfo->hour = hour;
            timeinfo->min = mins;

            QString timeformat = m_settings->GetSetting("TimeFormat");
            if (timeformat == "")
                timeformat = "h:mm AP";

            timeinfo->usertime = QTime(hour, mins).toString(timeformat);

            m_timeInfos[x] = timeinfo;
        }

        t = t.addSecs(5 * 60);
    }
    m_currentEndTime = t;
}

void GuideGrid::fillProgramInfos(void)
{
    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        fillProgramRowInfos(y);
    }
}

void GuideGrid::fillProgramRowInfos(unsigned int row)
{
    QPtrList<ProgramInfo> *proglist;
    ProgramInfo *program;

    proglist = m_programs[row];
    if (proglist)
    {
        for (program = proglist->first(); program; program = proglist->next())
        {
            delete program;
        }
        delete proglist;
    }

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        m_programInfos[row][x] = NULL;
    }

    int chanNum = row + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum < 0)
        chanNum = 0;

    if (m_channelInfos[chanNum].chanstr != "")
    {
        m_programs[row] = proglist = new QPtrList<ProgramInfo>;

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

        GetProgramRangeDateTime(proglist, chanid, starttime, endtime);

        QDateTime ts = m_currentStartTime;

        program = proglist->first();
        ProgramInfo *proginfo = NULL;
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
                    proginfo->title = m_settings->GetSetting("UnknownTitle");
                    proginfo->category = 
                                      m_settings->GetSetting("UnknownCategory");
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
    }
}

void GuideGrid::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    QPainter p(this);

    if (showInfo)
    {
    }
    else
    {
        if (r.intersects(infoRect()) && programGuideType == 1)
            updateTopInfo();
        if (r.intersects(dateRect()))
            paintDate(&p);
        if (r.intersects(channelRect()))
            paintChannels(&p);
        if (r.intersects(timeRect()))
            paintTimes(&p);
        if (r.intersects(programRect()))
            paintPrograms(&p);
        if (showtitle && r.intersects(titleRect()) && programGuideType != 1)
            paintTitle(&p);
    }
}

void GuideGrid::paintDate(QPainter *p)
{
    QRect dr = dateRect();
    QPixmap pix(dr.size());
    pix.fill(this, dr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
    tmp.setFont(*m_timeFont);

    QString date = m_currentStartTime.toString("ddd");
    QFontMetrics lfm(*m_timeFont);
    int datewidth = lfm.width(date);
    int dateheight = lfm.height();

    if (programGuideType != 1)
    {
        tmp.drawText((dr.width() - datewidth) / 2,
                     (dr.height() - dateheight) / 2 + dateheight / 3, date);

        date = m_currentStartTime.toString("MMM d");
        datewidth = lfm.width(date);

        tmp.drawText((dr.width() - datewidth) / 2,
                     (dr.height() - dateheight) / 2 + dateheight * 4 / 3, date);

        tmp.drawLine(0, dr.height() - 1, dr.right(), dr.height() - 1);
        tmp.drawLine(dr.width() - 1, 0, dr.width() - 1, dr.height());
    }
    else
    {
        date = m_currentStartTime.toString("MMM d");
        datewidth = lfm.width(date);

        // DATE BACKGROUND COLOR
        tmp.fillRect(0, 0, (int)(dr.width() - 2*wmult), 
                     (int)(dr.height() - 1*hmult),
                     QBrush(date_bgColor, SolidPattern));

        // DATE DROPSHADOW COLOR
        tmp.setPen(QPen(date_dsColor, (int)(2 * wmult)));
        tmp.drawText((int)((dr.width() - datewidth) / 2 + 2*wmult),
                     (int)((dr.height() - dateheight) / 2 + dateheight - 
                     2*hmult), date);

        // DATE FOREGROUND COLOR
        tmp.setPen(QPen(date_fgColor, (int)(2 * wmult)));
        tmp.drawText((dr.width() - datewidth) / 2,
                     (dr.height() - dateheight) / 2 + dateheight - 4, date);
    }


    tmp.end();

    p->drawPixmap(dr.topLeft(), pix);
}

void GuideGrid::paintChannels(QPainter *p)
{
    QRect cr = channelRect();
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
    tmp.setFont(*m_chanFont);

    int ydifference = cr.height() / DISPLAY_CHANS;

    for (unsigned int y = 0; y < (unsigned int)DISPLAY_CHANS; y++)
    {
        unsigned int chanNumber = y + m_currentStartChannel;
        if (chanNumber >= m_channelInfos.size())
            chanNumber -= m_channelInfos.size();
  
        if (m_channelInfos[chanNumber].chanstr == "")
            break;

        ChannelInfo *chinfo = &(m_channelInfos[chanNumber]);
        tmp.setFont(*m_chanFont);
        QFontMetrics lfm(*m_chanFont);
        int bheight = lfm.height();

        if (programGuideType != 1)
        {
            if (chinfo->iconpath != "none" && chinfo->iconpath != "" && 
                showIcon)
            {
                if (!chinfo->icon)
                    chinfo->LoadIcon(m_context);
                if (chinfo->icon)
                {
                    int yoffset = (int)(4 * hmult);
                    tmp.drawPixmap((cr.width() - chinfo->icon->width()) / 2, 
                                   ydifference * y + yoffset, *(chinfo->icon));
                }
            }
         
            int width = lfm.width(chinfo->chanstr);
       
            int yoffset = 0;
            if (showIcon)
                yoffset = (int)(43 * hmult); 
            tmp.drawText((cr.width() - width) / 2, 
                         ydifference * y + yoffset + bheight, chinfo->chanstr);

            tmp.setFont(*m_chanCallsignFont);
            QFontMetrics fm(*m_chanCallsignFont);
            width = fm.width(chinfo->callsign);
            int height = fm.height();
            tmp.drawText((cr.width() - width) / 2, 
                         ydifference * y + yoffset + bheight + height, 
                         chinfo->callsign);

            tmp.drawLine(0, ydifference * (y + 1), cr.right(), 
                         ydifference * (y + 1));
        }
        else
        {
            QString chData = chinfo->chanstr + " " + chinfo->callsign;
            int width = lfm.width(chData);

            // CHANNEL BACKGROUND COLOR
            tmp.fillRect(0, (int)(ydifference * y + 1*hmult), 
                         (int)(cr.width() - 2*wmult), 
                         (int)(ydifference - 2*hmult),
                         QBrush(chan_bgColor, SolidPattern));

            // CHANNEL DROPSHADOW COLOR
            tmp.setPen(QPen(chan_dsColor, (int)(2 * wmult)));
            tmp.drawText((int)((cr.width() - width) / 2 + 2*wmult),
                         (int)(ydifference * y + bheight + 2*hmult), chData);

            // CHANNEL FOREGROUND COLOR
            tmp.setPen(QPen(chan_fgColor, (int)(2 * wmult)));
            tmp.drawText((cr.width() - width) / 2,
                         ydifference * y + bheight, chData);
        }
    }

    if (programGuideType != 1)
        tmp.drawLine(cr.right(), 0, cr.right(), DISPLAY_CHANS * ydifference);

    tmp.end();
    
    p->drawPixmap(cr.topLeft(), pix);
}

void GuideGrid::paintTimes(QPainter *p)
{
    QRect tr = timeRect();
    QPixmap pix(tr.size());
    pix.fill(this, tr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
    tmp.setFont(*m_timeFont);

    int xdifference = (int)(tr.width() / DISPLAY_TIMES); 
    QDateTime now = QDateTime::currentDateTime();
    int nowpos = -9999;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x])
        {
            if (programGuideType != 1)
            {
                tmp.drawLine((x + 6) * xdifference, 0, (x + 6) * xdifference, 
                             tr.bottom());
            }
            else
            {
                tmp.fillRect((int)((x + 6) * xdifference - 4*wmult), 0,
                             -(int)(xdifference*6 - 2*wmult), tr.height(),
                             QBrush(time_bgColor, SolidPattern));
            }
        }
    }

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x])
        {
            TimeInfo *tinfo = m_timeInfos[x];

            QFontMetrics fm(*m_timeFont);
            int width = fm.width(tinfo->usertime);
            int height = fm.height();

            int xpos = (x * xdifference) + (xdifference * 6 - width) / 2; 
            if (programGuideType != 1)
            {
                tmp.drawText(xpos, (tr.bottom() - height) / 2 + height, 
                             tinfo->usertime);
            }
            else
            {
                if (x == 0)
                {
                    firstTime = m_currentStartTime;
                    lastTime = firstTime.addSecs(DISPLAY_TIMES * 60 * 4); 
                }

                int xpos = (x * xdifference) + (xdifference * 6 - width) / 2;

                // TIME DROPSHADOW COLOR
                tmp.setPen(QPen(time_dsColor, (int)(2 * wmult)));
                tmp.drawText((int)(xpos + 2*wmult), 
                             (int)((tr.bottom() - tr.top()) / 2 + 
                             (height / 2) - 1*hmult), tinfo->usertime);

                // TIME FOREGROUND COLOR
                tmp.setPen(QPen(time_fgColor, (int)(2 * wmult)));
                tmp.drawText(xpos, (int)((tr.bottom() - tr.top()) / 2 + 
                             (height / 2) - 3*hmult),
                             tinfo->usertime);

            }

            int t = now.secsTo(QDateTime(m_currentStartTime.date(),
                                         QTime(tinfo->hour, tinfo->min)));
            if (nowpos == -9999)
                nowpos = x + (t / -60 / 5);
        }
    }

    if (programGuideType != 1)
        tmp.drawLine(0, tr.height() - 1, tr.right(), tr.height() - 1);

    if (m_settings->GetNumSetting("EPGShowCurrentTime"))
    {
        QColor color = QColor(m_settings->GetSetting("EPGCurrentTimeColor"));
        if (!color.isValid())
            color.setRgb(255,0,0);
        
        QFontMetrics fm(*m_timeFont);
        int height = fm.height();
        
        if (nowpos < 0)
        {
            tmp.setPen(QPen(color, (int)(2 * wmult)));
            tmp.drawText(0, (tr.bottom() - height) / 2 + height, "<<");
        }
        else if (nowpos >= 0 && nowpos < DISPLAY_TIMES)
        {
            tmp.setPen(QPen(color, (int)(2 * wmult)));
            tmp.drawLine((nowpos) * xdifference, 0, (nowpos) * xdifference, 
                         tr.bottom());

            QString nows;
            QString timeformat = m_settings->GetSetting("TimeFormat", 
                                                        "h:mm AP");
            nows.sprintf(now.time().toString(timeformat));
            tmp.drawText((nowpos) * xdifference + 3, tr.bottom() - 3, nows);
        }
        else
        {
            tmp.setPen(QPen(color, (int)(2 * wmult)));
            tmp.drawText(xdifference * (DISPLAY_TIMES - 1), 
                         (tr.bottom() - height) / 2 + height, ">>");
        }
    }
   
    tmp.end();

    p->drawPixmap(tr.topLeft(), pix);
}

QBrush GuideGrid::getBGColor(const QString &category)
{
    QBrush br = QBrush(bgcolor);
  
    if (!usetheme)
        return br;

    QString cat = "Cat_" + category;

    QString color = m_context->qtconfig()->GetSetting(cat);
    if (color != "")
    {
        br = QBrush(color);
    }

    return br;
}

QRgb blendColors(QRgb source, QRgb add, int alpha)
{
    int sred = qRed(source);
    int sgreen = qGreen(source);
    int sblue = qBlue(source);

    int tmp1 = (qRed(add) - sred) * alpha;
    int tmp2 = sred + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sred = tmp2 & 0xff;

    tmp1 = (qGreen(add) - sgreen) * alpha;
    tmp2 = sgreen + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sgreen = tmp2 & 0xff;

    tmp1 = (qBlue(add) - sblue) * alpha;
    tmp2 = sblue + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
    sblue = tmp2 & 0xff;

    return qRgb(sred, sgreen, sblue);
}


void GuideGrid::paintPrograms(QPainter *p)
{
    QRect pr = programRect();
    QPixmap pix(pr.size());
    pix.fill(this, pr.topLeft());

    QImage bgimage = pix.convertToImage();

    QPainter tmp(&pix);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));

    tmp.setFont(*m_progFont);

    int ydifference = pr.height() / DISPLAY_CHANS;
    int xdifference = pr.width() / DISPLAY_TIMES;

    if (programGuideType != 1)
    {
        for (int y = 1; y < DISPLAY_CHANS + 1; y++)
        {
            int ypos = ydifference * y;
            tmp.drawLine(0, ypos, pr.right(), ypos);
        }
    }

    float tmpwmult = wmult;
    if (tmpwmult < 1)
        tmpwmult = 1;
    float tmphmult = hmult;
    if (tmphmult < 1)
        tmphmult = 1;
 
    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        unsigned int chanNum = y + m_currentStartChannel;
        if (chanNum >= m_channelInfos.size())
            chanNum -= m_channelInfos.size();

        if (m_channelInfos[chanNum].chanstr == "")
            break;

        QDateTime lastprog;
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            ProgramInfo *pginfo = m_programInfos[y][x];

            if (pginfo->recordtype == kUnknown)
                pginfo->GetProgramRecordingStatus();

            int spread = 1;
            if (pginfo->startts != lastprog)
            {
                QFontMetrics fm(*m_progFont);
                int height = fm.height();

                if (pginfo->spread != -1)
                {
                    spread = pginfo->spread;
                }
                else
                {
                    for (int z = x + 1; z < DISPLAY_TIMES; z++)
                    {
                        if (m_programInfos[y][z]->startts == pginfo->startts)
                            spread++;
                    }
                    pginfo->spread = spread;
                    pginfo->startCol = x;

                    for (int z = x + 1; z < x + spread; z++)
                    {
                        m_programInfos[y][z]->spread = spread;
                        m_programInfos[y][z]->startCol = x;
                        m_programInfos[y][z]->recordtype = pginfo->recordtype;
                    }
                }

                QBrush br = getBGColor(pginfo->category);

                if (br != QBrush(bgcolor))
                {
                    QRgb blendcolor = br.color().rgb();

                    int startx = (int)(x * xdifference + 1 * tmpwmult);
                    int endx = (int)((x + spread) * xdifference - 2 * tmpwmult);
                    int starty = (int)(ydifference * y + 1 * tmphmult);
                    int endy = (int)(ydifference * (y + 1) - 2 * tmphmult);
 
                    if (x == 0)
                        startx--;
                    if (y == 0)
                        starty--;

                    if (startx < 0)
                        startx = 0;
                    if (starty < 0)
                        starty = 0;
                    if (endx > pr.right())
                        endx = pr.right();
                    if (endy > pr.bottom())
                        endy = pr.bottom();

                    unsigned int *data = NULL;
                   
                    for (int tmpy = starty; tmpy <= endy; tmpy++)
                    {
                        data = (unsigned int *)bgimage.scanLine(tmpy);
                        for (int tmpx = startx; tmpx <= endx; tmpx++)
                        {
                            QRgb pixelcolor = data[tmpx];
                            data[tmpx] = blendColors(pixelcolor, blendcolor, 
                                                     96);
                        }
                    }
                    tmp.drawImage(startx, starty, bgimage, startx, starty, 
                                  endx - startx + 1, 
                                  endy - starty + 1);
                }

                int maxwidth = (int)(spread * xdifference - (10 * wmult));

                QString info = pginfo->title;
                if (pginfo->category != "" && pginfo->title != "" && usetheme)
                    info += " (" + pginfo->category + ")";

                if (programGuideType == 1)
                {
                    if (pginfo->startts < firstTime.addSecs(-300))
                    {
                        info = "  " + info;
                    }
                    if (pginfo->endts > lastTime.addSecs(2100))
                    {
                        info = info + "    ";
                    }
                }
                
                int startx = (int)(x * xdifference + 7 * wmult);
                int kstartx = startx;

                if (programGuideType != 1)
                {
                    tmp.drawText(startx,
                                (int)(height / 8 + y * ydifference + 1 * hmult),
                                 maxwidth, ydifference - height / 8,
                                 AlignLeft | WordBreak,
                                 info);
                }

                tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));

                startx = (int)((x + spread) * xdifference); 

                int ystart = 1;
                int rectheight = (int)(ydifference - 3 * tmphmult);
                if (y != 0)
                {
                    ystart = (int)(ydifference * y + 2 * tmphmult);
                    rectheight = (int)(ydifference - 4 * tmphmult);
                }

                int xstart = 1;
                if (x != 0)
                    xstart = (int)(x * xdifference + 2 * tmpwmult);
                int xend = (int)(xdifference * spread - 4 * tmpwmult);
                if (x == 0)
                    xend += (int)(1 * tmpwmult);

                if (programGuideType != 1)
                {
                    tmp.drawLine(startx, ydifference * y, startx,
                                 ydifference * (y + 1));

                    if (pginfo->recordtype > kNotRecording)
                    {
                        tmp.setPen(QPen(red, (int)(2 * wmult)));
                        QString text;

                        if (pginfo->recordtype == kSingleRecord)
                            text = "R";
                        else if (pginfo->recordtype == kTimeslotRecord)
                            text = "T";
                        else if (pginfo->recordtype == kChannelRecord)
                            text = "C";
                        else if (pginfo->recordtype == kAllRecord)
                            text = "A";

                        int width = fm.width(text);

                        startx = (int)((x + spread) * xdifference - 
                                       width * 1.5);
                        tmp.drawText(startx, (y + 1) * ydifference - height,
                                     (int)(width * 1.5), height, AlignLeft, 
                                     text);

                        tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
                    }

                    if (m_currentRow == (int)y)
                    {
                        if ((m_currentCol >= x) && 
                            (m_currentCol < (x + spread)))
                        {
                            tmp.setPen(QPen(red, (int)(3.75 * wmult)));

                            int ystart = 1;
                            int rectheight = (int)(ydifference - 3 * tmphmult);
                            if (y != 0)
                            {
                                ystart = (int)(ydifference * y + 2 * tmphmult);
                                rectheight = (int)(ydifference - 4 * tmphmult);
                            }

                            int xstart = 1;
                            if (x != 0)
                                xstart = (int)(x * xdifference + 2 * tmpwmult);
                            int xend = (int)(xdifference * spread - 4 * 
                                             tmpwmult);
                            if (x == 0)
                                xend += (int)(1 * tmpwmult);
	
                            tmp.drawRect(xstart, ystart, xend, rectheight);
                            tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
                         }
                    }
                }
                else
                {
                    int rrOffset = 5;
                    int rRound = 12;

                    // PROGRAM BACKGROUND COLOR
                    tmp.fillRect(xstart - (rrOffset / 2), 
                                 ystart - (rrOffset / 2),
                                 xend + (rrOffset / 2), 
                                 rectheight + (rrOffset / 2),
                                 QBrush(prog_bgColor, SolidPattern));

                    // PROGRAM LINE COLOR
                    tmp.setPen(QPen(progLine_Color, (int)(.5 * wmult)));
                    tmp.drawRoundRect(xstart - rrOffset, ystart - rrOffset,
                                      xend + rrOffset, rectheight + rrOffset, 
                                      rRound, rRound);

                    bool isCurrent = false;

                    if (m_currentRow == (int)y)
                    {
                        if ((m_currentCol >= x) && 
                            (m_currentCol < (x + spread)))
                        {
                            isCurrent = true;
                            tmp.setPen(QPen(red, (int)(3.75 * wmult)));

                            int ystart = 1;
                            int rectheight = (int)(ydifference - 3 * tmphmult);
                            if (y != 0)
                            {
                                ystart = (int)(ydifference * y + 2 * tmphmult);
                                rectheight = (int)(ydifference - 4 * tmphmult);
                            }

                            int xstart = 1;
                            if (x != 0)
                                xstart = (int)(x * xdifference + 2 * tmpwmult);
                            int xend = (int)(xdifference * spread - 4 * 
                                             tmpwmult);
                            if (x == 0)
                                xend += (int)(1 * tmpwmult);

                            if (pginfo->recordtype == kNotRecording)
                            {
                                // CURRENT PROGRAM HIGHLIGHT COLOR
                                tmp.fillRect(xstart - (rrOffset / 2), 
                                             ystart - (rrOffset / 2),
                                             xend + (rrOffset / 2), 
                                             rectheight + (rrOffset / 2),
                                             QBrush(curProg_bgColor, 
                                             SolidPattern));
                            }
                            else
                            {
                                // CURRENT PROGRAM BEING RECORDED HIGHLIGHT COLOR
                                tmp.fillRect(xstart - (rrOffset / 2), 
                                             ystart - (rrOffset / 2),
                                             xend + (rrOffset / 2), 
                                             rectheight + (rrOffset / 2),
                                             QBrush(curRecProg_bgColor, 
                                             SolidPattern));
                            }
                            // PROGRAM LINE COLOR
                            tmp.setPen(QPen(progLine_Color, 
                                            (int)(1.5 * wmult)));
                            tmp.drawRoundRect(xstart - rrOffset, 
                                              ystart - rrOffset,
                                              xend + rrOffset, 
                                              rectheight + rrOffset, rRound, 
                                              rRound);
                        }
                    }

                    if (pginfo->recordtype > kNotRecording)
                    {
                        tmp.setPen(QPen(yellow, (int)(2 * wmult)));
                        QString text;

                        if (pginfo->recordtype == kSingleRecord)
                            text = "R";
                        else if (pginfo->recordtype == kTimeslotRecord)
                            text = "T";
                        else if (pginfo->recordtype == kChannelRecord)
                            text = "C";
                        else if (pginfo->recordtype == kAllRecord)
                            text = "A";

                        int width = fm.width(text);

                        startx = (int)((x + spread) * xdifference - 
                                       width * 1.5);
                        tmp.drawText(startx, (y + 1) * ydifference - height,
                                     (int)(width * 1.5), height, AlignLeft, 
                                     text);

                        tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
                    }
                    /*
                        the following code adds the ... to program names
                        that are too long for the program box
                     */
                    int curFontWidth = fm.width(info);
                    if (curFontWidth > maxwidth)
                    {
                        QString testInfo = "";
                        curFontWidth = fm.width(testInfo);
                        int tmaxwidth = maxwidth - fm.width("LLL");
                        int count = 0;

                        while (curFontWidth < tmaxwidth)
                        {
                            testInfo = info.left(count);
                            curFontWidth = fm.width(testInfo);
                            count = count + 1;
                        }
                        testInfo = testInfo + "...";
                        info = testInfo;
                    }

                    if (isCurrent == true)
                    {
                        // CURRENT PROGRAM DROPSHADOW COLOR
                        tmp.setPen(QPen(curProg_dsColor, (int)(1.5 * wmult)));
                        tmp.drawText((int)(kstartx + 1*wmult),
                                     (int)(height / 8 + y * ydifference + 
                                     1 * hmult + 1*hmult),
                                     maxwidth, (ydifference - height / 8),
                                     AlignLeft,
                                     info);
                        // CURRENT PROGRAM FOREGROUND COLOR
                        tmp.setPen(QPen(curProg_fgColor, (int)(1.5 * wmult)));
                    }
                    else
                    {
                        // PROGRAM FOREGROUND COLOR
                        tmp.setPen(QPen(prog_fgColor, (int)(2 * wmult)));
                    }

                    tmp.drawText(kstartx,
                                 (int)(height / 8 + y * ydifference + 
                                 1 * hmult),
                                 maxwidth, ydifference - height / 8, AlignLeft,
                                 info);

                    if (pginfo->startts < firstTime.addSecs(-300))
                    {
                        // PROGRAM ARROW TYPE
                        if (progArrow_Type == 0)
                        {
                            // PROGRAM ARROW COLOR
                            tmp.setPen(QPen(progArrow_Color, 
                                            (int)(4.25 * wmult)));
                            tmp.drawLine((int)(xstart + 8*wmult), 
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         3 * hmult),
                                         (int)(xstart + 8*wmult), 
                                         (int)(ystart + (int)(rectheight / 2) + 
                                         3 * hmult));
                            tmp.drawLine((int)(xstart + 10*wmult), ystart,
                                         (int)(xstart + 10*wmult), 
                                         (int)(ystart + rectheight - 2*hmult));
                            tmp.drawLine((int)(xstart + 10*wmult), ystart,
                                         (int)(xstart + 4*wmult), 
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                            tmp.drawLine((int)(xstart + 10*wmult), 
                                         (int)(ystart + rectheight - 2*hmult),
                                         (int)(xstart + 4*wmult), 
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                        }
                        if (progArrow_Type == 1)
                        {
                            tmp.setPen(QPen(progArrow_Color, (int)(2 * wmult)));
                            tmp.drawLine((int)(xstart + 10*wmult), ystart,
                                         (int)(xstart + 4*wmult), 
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                            tmp.drawLine((int)(xstart + 10*wmult), 
                                         (int)(ystart + rectheight - 2*hmult),
                                         (int)(xstart + 4*wmult), 
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                        }
                        if (progArrow_Type == 2)
                        {
                            tmp.setPen(QPen(progArrow_Color, 
                                            (int)(1.25 * wmult)));
                            tmp.drawLine((int)(xstart + 8*wmult), 
                                         ystart + (int)(rectheight / 4),
                                         (int)(xstart + 4*wmult), 
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                            tmp.drawLine((int)(xstart + 8*wmult),
                                         (int)(ystart + rectheight - 2*hmult - 
                                         (int)(rectheight / 4)),
                                         (int)(xstart + 4*wmult), ystart + 
                                         (int)((rectheight / 2) - 1*hmult));
                        }
                    }
                    if (pginfo->endts > lastTime.addSecs(2100))
                    {
                        // PROGRAM ARROW TYPE
                        if (progArrow_Type == 0)
                        {
                            // PROGRAM ARROW COLOR
                            tmp.setPen(QPen(progArrow_Color, 
                                            (int)(4.25 * wmult)));
                            tmp.drawLine((int)(xstart + xend - 8*wmult), 
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         6*hmult),
                                         (int)(xstart + xend - 8*wmult),
                                         (int)(ystart + (int)(rectheight / 2) + 
                                         6*hmult));
                            tmp.drawLine((int)(xstart + xend - 10*wmult), 
                                         ystart,
                                         (int)(xstart + xend - 10*wmult), 
                                         (int)(ystart + rectheight - 2*hmult));
                            tmp.drawLine((int)(xstart + xend - 10*wmult), 
                                         ystart,
                                         (int)(xstart + xend - 4*wmult),
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                            tmp.drawLine((int)(xstart + xend - 10*wmult), 
                                         (int)(ystart + rectheight - 2*hmult),
                                         (int)(xstart + xend - 4*wmult),
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                        }
                        if (progArrow_Type == 1)
                        {
                            tmp.setPen(QPen(progArrow_Color, (int)(2 * wmult)));
                            tmp.drawLine((int)(xstart + xend - 10*wmult), 
                                         ystart,
                                         (int)(xstart + xend - 4*wmult),
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                            tmp.drawLine((int)(xstart + xend - 10*wmult), 
                                         (int)(ystart + rectheight - 2*hmult),
                                         (int)(xstart + xend - 4*wmult),
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                        }
                        if (progArrow_Type == 2)
                        {
                            tmp.setPen(QPen(progArrow_Color, 
                                            (int)(1.25 * wmult)));
                            tmp.drawLine((int)(xstart + xend - 10*wmult), 
                                         (int)(ystart + (int)(rectheight / 4)),
                                         (int)(xstart + xend - 4*wmult),
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                            tmp.drawLine((int)(xstart + xend - 10*wmult),
                                         (int)(ystart + rectheight - 2*hmult - 
                                         (int)(rectheight / 4)),
                                         (int)(xstart + xend - 4*wmult),
                                         (int)(ystart + (int)(rectheight / 2) - 
                                         1*hmult));
                        }
                    }
                }
            }
            lastprog = pginfo->startts;
        }
    }

    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
}

void GuideGrid::updateTopInfo()
{
    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];
    
  
    titlefield->setText(" " + pginfo->title);
    date->setText(getDateLabel(pginfo));
    subtitlefield->setText(pginfo->subtitle);
    descriptionfield->setText(pginfo->description);

    if (pginfo->recordtype == kNotRecording)
        recordingfield->setText("Not Recording");
    else if (pginfo->recordtype == kSingleRecord)
        recordingfield->setText("Recording Once");
    else if (pginfo->recordtype == kTimeslotRecord)
        recordingfield->setText("Timeslot Recording");
    else if (pginfo->recordtype == kChannelRecord)
        recordingfield->setText("Channel Recording");
    else if (pginfo->recordtype == kAllRecord)
        recordingfield->setText("All Recording");

    int chanNum = m_currentRow + m_currentStartChannel;
    if (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();
    if (chanNum < 0)
        chanNum = 0;

    ChannelInfo *chinfo = &(m_channelInfos[chanNum]);

    if (chinfo->iconpath != "none" && chinfo->iconpath != "" && showIcon)
    {
        if (!chinfo->icon)
            chinfo->LoadIcon(m_context);
        if (chinfo->icon)
        {
            channelimage->setText("");
            channelimage->setMinimumWidth(chinfo->icon->width());
            channelimage->setMaximumWidth(chinfo->icon->width());
            channelimage->setMinimumHeight(chinfo->icon->height());
            channelimage->setPaletteBackgroundPixmap(*(chinfo->icon));
        }
    }
    else
    {
        // MISSING CHANNEL ICON BACKGROUND COLOR
        channelimage->setPaletteBackgroundColor(misChanIcon_bgColor);
        channelimage->setText(pginfo->chansign);
    }
}

void GuideGrid::paintTitle(QPainter *p)
{
    QRect tr = titleRect();
    QPixmap pix(tr.size());
    pix.fill(this, tr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
    tmp.setFont(*m_titleFont);

    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];
    QFontMetrics lfm(*m_titleFont);
    int titleheight = lfm.height();

    QString info = pginfo->title;
    if (pginfo->category != "" && usetheme)
       info += " (" + pginfo->category + ")";

    tmp.drawText((tr.height() - titleheight) / 2 , 
                 (tr.height() - titleheight) / 2 + titleheight,
                 info);

    tmp.end();
    
    p->drawPixmap(tr.topLeft(), pix);
}

QRect GuideGrid::fullRect() const
{
    QRect r(0, 0, (int)(800 * wmult), (int)(600 * hmult));
    return r;
}

QRect GuideGrid::dateRect() const
{
    QRect r;
    if (programGuideType != 1)
        r = QRect(0, 0, programRect().left(), programRect().top());
    else
        r = QRect(0, (int)(300 * wmult), programRect().left(), 
                  (int)(26 * wmult));

    return r;
}

QRect GuideGrid::channelRect() const
{
    QRect r;
    if (programGuideType != 1)
        r = QRect(0, dateRect().height(), dateRect().width(), 
                  programRect().height());
    else
        r = QRect(0, programRect().top(), dateRect().width(), 
                  programRect().height());

    return r;
}

QRect GuideGrid::timeRect() const
{
    QRect r;
    if (programGuideType != 1)
        r = QRect(dateRect().width(), 0, programRect().width(), 
                  dateRect().height());
    else
        r = QRect(dateRect().width(), dateRect().top(), programRect().width(), 
                  (int)(dateRect().height() - 1*hmult));

    return r;
}

QRect GuideGrid::programRect() const
{
    QRect r;
    if (programGuideType != 1)
    {
        // Change only these numbers to adjust the size of the visible regions

        unsigned int min_dateheight = 50;  // also min_timeheight
        unsigned int min_datewidth = 74;   // also min_chanwidth
        unsigned int titleheight = showtitle ? 40 : 0;

        unsigned int programheight = (int)((600 - min_dateheight - 
                                           titleheight) * wmult);
        programheight = DISPLAY_CHANS * (int)(programheight / DISPLAY_CHANS);

        unsigned int programwidth = (int)((800 - min_datewidth) * hmult);
        programwidth = DISPLAY_TIMES * (int)(programwidth / DISPLAY_TIMES);

        r = QRect((int)(800 * wmult) - programwidth, 
                  (int)(600 * hmult) - programheight - titleheight,
                  programwidth, programheight);
    }
    else
    {
        // Change only these numbers to adjust the size of the visible regions
        unsigned int min_dateheight = 25;  // also min_timeheight
        unsigned int min_datewidth = 100;   // also min_chanwidth

        unsigned int programheight = (int)((300 - min_dateheight + 1) *
                                           wmult);

        programheight = DISPLAY_CHANS * (int)(programheight / DISPLAY_CHANS);

        unsigned int programwidth = (int)((800 - min_datewidth) * hmult);
        programwidth = DISPLAY_TIMES * (int)(programwidth / DISPLAY_TIMES);

        r = QRect((int)(800 * wmult) - programwidth,
                  (int)(600 * hmult) - programheight,
                  programwidth, programheight);
    }

    return r;
}

QRect GuideGrid::infoRect() const
{
    QRect r(0, 0, (int)(400 * wmult), (int)(300 * hmult));
    return r;
}

QRect GuideGrid::titleRect() const
{
    QRect r(0, programRect().bottom() + 1, fullRect().width(), 
            fullRect().height() - programRect().bottom());
    return r;
}

void GuideGrid::cursorLeft()
{
    int startCol = m_programInfos[m_currentRow][m_currentCol]->startCol;

    m_currentCol = startCol - 1;

    if (m_currentCol < 0)
    {
        m_currentCol = 0;
        emit scrollLeft();
    }
    else
    {
        update(programRect());
        if (showtitle)
            update(titleRect());
        if (programGuideType == 1)
            update(infoRect());
    }
}

void GuideGrid::cursorRight()
{
    int spread = m_programInfos[m_currentRow][m_currentCol]->spread;
    int startCol = m_programInfos[m_currentRow][m_currentCol]->startCol;

    m_currentCol = startCol + spread;

    if (m_currentCol > DISPLAY_TIMES - 1)
    {
        m_currentCol = DISPLAY_TIMES - 1;
        emit scrollRight();
    }
    else
    {
        update(programRect());
        if (showtitle)
            update(titleRect());
        if (programGuideType == 1)
            update(infoRect());
    }
}

void GuideGrid::cursorDown()
{
    m_currentRow++;

    if (m_currentRow > DISPLAY_CHANS - 1)
    {
        m_currentRow = DISPLAY_CHANS - 1;
        emit scrollDown();
    }
    else
    {
        update(programRect());
        if (showtitle)
            update(titleRect());
        if (programGuideType == 1)
            update(infoRect());
    }
}

void GuideGrid::cursorUp()
{
    m_currentRow--;

    if (m_currentRow < 0)
    {
        m_currentRow = 0;
        emit scrollUp();
    }
    else
    {
        update(programRect());
        if (showtitle)
            update(titleRect());
        if (programGuideType == 1)
            update(infoRect());
    }
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

    update(timeRect());
    if (updatedate) 
        update(dateRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
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

    update(timeRect());
    if (updatedate)
        update(dateRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
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
    fillProgramRowInfos(DISPLAY_CHANS - 1);

    update(channelRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
}

void GuideGrid::scrollUp()
{
    setStartChannel(m_currentStartChannel - 1);

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
    fillProgramRowInfos(0);

    update(channelRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
}

void GuideGrid::dayLeft()
{
    m_currentStartTime = m_currentStartTime.addSecs(-24 * 60 * 60);

    fillTimeInfos();
    fillProgramInfos();

    update(dateRect());
    update(timeRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
}

void GuideGrid::dayRight()
{
    m_currentStartTime = m_currentStartTime.addSecs(24 * 60 * 60);

    fillTimeInfos();
    fillProgramInfos();

    update(dateRect());
    update(timeRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
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

    update(timeRect());
    if (updatedate)
        update(dateRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
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

    update(timeRect());
    if (updatedate)
        update(dateRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
}

void GuideGrid::pageDown()
{
    setStartChannel(m_currentStartChannel + DISPLAY_CHANS);

    fillProgramInfos();

    update(channelRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
}

void GuideGrid::pageUp()
{
    setStartChannel(m_currentStartChannel - DISPLAY_CHANS);

    fillProgramInfos();

    update(channelRect());
    update(programRect());
    if (showtitle)
        update(titleRect());
    if (programGuideType == 1)
        update(infoRect());
}

void GuideGrid::enter()
{
    unsetCursor();
    selectState = 1;
    emit killTheApp();
}

void GuideGrid::escape()
{
    unsetCursor();
    emit killTheApp();
}

void GuideGrid::displayInfo()
{
    showInfo = 1;

    ProgramInfo *pginfo = m_programInfos[m_currentRow][m_currentCol];

    if (pginfo)
    {
        InfoDialog diag(m_context, pginfo, this, "Program Info");
        diag.setCaption("BLAH!!!");
        diag.exec();
    }

    showInfo = 0;

    pginfo->GetProgramRecordingStatus();

    int startcol = pginfo->startCol;
    int spread = pginfo->spread;
    RecordingType rectype = pginfo->recordtype;

    for (int i = 0; i < spread; i++)
    {
        m_programInfos[m_currentRow][startcol + i]->recordtype = rectype;
    }

    setActiveWindow();
    setFocus();

    fillProgramInfos();
    update(programRect());
}
