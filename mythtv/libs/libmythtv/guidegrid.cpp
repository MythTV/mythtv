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

#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "guidegrid.h"
#include "infodialog.h"
#include "infostructs.h"
#include "programinfo.h"
#include "settings.h"

using namespace libmyth;

GuideGrid::GuideGrid(MythContext *context, const QString &channel, 
                     QWidget *parent, const char *name)
         : QDialog(parent, name)
{
    m_context = context;
    m_settings = context->settings();

    m_context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setCursor(QCursor(Qt::BlankCursor));

    usetheme = m_settings->GetNumSetting("ThemeQt");
    showtitle = m_settings->GetNumSetting("EPGShowTitle");

    m_context->ThemeWidget(this, screenwidth, screenheight, wmult, hmult);

    bgcolor = paletteBackgroundColor();
    fgcolor = paletteForegroundColor();

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

    int secsoffset = -(m_originalStartTime.time().minute() % 30) * 60;
    m_currentStartTime = m_originalStartTime.addSecs(secsoffset);
    m_currentStartChannel = 0;
    m_startChanStr = channel;

    m_currentRow = 0;
    m_currentCol = 0;

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

    QAccel *accel = new QAccel(this);
    accel->connectItem(accel->insertItem(Key_Left), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_Right), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_Down), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_Up), this, SLOT(cursorUp()));

    accel->connectItem(accel->insertItem(Key_A), this, SLOT(cursorLeft()));
    accel->connectItem(accel->insertItem(Key_D), this, SLOT(cursorRight()));
    accel->connectItem(accel->insertItem(Key_S), this, SLOT(cursorDown()));
    accel->connectItem(accel->insertItem(Key_W), this, SLOT(cursorUp()));

    accel->connectItem(accel->insertItem(Key_C), this, SLOT(escape()));
    accel->connectItem(accel->insertItem(Key_Escape), this, SLOT(escape()));
    accel->connectItem(accel->insertItem(Key_Enter), this, SLOT(enter()));
    accel->connectItem(accel->insertItem(Key_M), this, SLOT(enter()));

    accel->connectItem(accel->insertItem(Key_I), this, SLOT(displayInfo()));
    accel->connectItem(accel->insertItem(Key_Space), this, SLOT(displayInfo()));

    connect(this, SIGNAL(killTheApp()), this, SLOT(accept()));

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

        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            if (m_programInfos[y][x])
                delete m_programInfos[y][x];
        }
    }

    m_channelInfos.clear();

    delete m_timeFont;
    delete m_chanFont;
    delete m_chanCallsignFont;
    delete m_progFont;
    delete m_titleFont;
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

    char temp[512];
    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        TimeInfo *timeinfo = new TimeInfo;

        int year = t.date().year();
        int month = t.date().month();
        int day = t.date().day();

        int hour = t.time().hour();
        int mins = t.time().minute();

        int sqlmins;        

        mins = 5 * (mins / 5);
        sqlmins = mins + 2;

        bool am = true;
        if (hour >= 12)
            am = false;

        timeinfo->year = year;
        timeinfo->month = month;
        timeinfo->day = day;
        timeinfo->hour = hour;
        timeinfo->min = mins;

        QString timeformat = m_settings->GetSetting("TimeFormat");
        if (timeformat == "")
            timeformat = "h:mm AP";

        timeinfo->usertime = QTime(hour, mins).toString(timeformat);

        sprintf(temp, "%4d%02d%02d%02d%02d50", year, month, day, hour, sqlmins);
        timeinfo->sqltime = temp;

        m_timeInfos[x] = timeinfo;

        t = t.addSecs(5 * 60);
    }
}

ProgramInfo *GuideGrid::getProgramInfo(unsigned int row, unsigned int col)
{
    int chanNum = row + m_currentStartChannel;

    if (m_channelInfos.size() == 0)
        return NULL;

    while (chanNum >= (int)m_channelInfos.size())
        chanNum -= (int)m_channelInfos.size();

    if (chanNum < 0)
        chanNum = 0;

    if (m_channelInfos[chanNum].chanstr == "" || !m_timeInfos[col])
        return NULL;

    QString chanid = QString("%1").arg(m_channelInfos[chanNum].chanid);

    ProgramInfo *pginfo = GetProgramAtDateTime(chanid,
                                             m_timeInfos[col]->sqltime.ascii());
    if (pginfo)
    {
        pginfo->startCol = col;
        return pginfo;
    }
  
    ProgramInfo *proginfo = new ProgramInfo;
    proginfo->title = "Unknown"; 
    TimeInfo *tinfo = m_timeInfos[col];
    QDateTime ts;
    ts.setDate(QDate(tinfo->year, tinfo->month, tinfo->day));
    ts.setTime(QTime(tinfo->hour, tinfo->min));

    proginfo->spread = 1;
    proginfo->startCol = col;
    proginfo->startts = ts;
    proginfo->endts = proginfo->startts.addSecs(5 * 60);

    for (int newcol = col; newcol % 6; newcol--)
    {
        pginfo = GetProgramAtDateTime(chanid, 
                                      m_timeInfos[newcol]->sqltime.ascii());

        if (pginfo)
        {
            delete pginfo;
            break;
        }
        else
        {
            proginfo->startts = proginfo->startts.addSecs(-5 * 60);
            proginfo->startCol--;
            proginfo->spread++;
        }
    }

    for (int newcol = col; newcol % 6 < 5; newcol++)
    {
        pginfo = GetProgramAtDateTime(chanid,
                                      m_timeInfos[newcol]->sqltime.ascii());

        if (pginfo)
        {
            delete pginfo;
            break;
        }
        else
        {
            proginfo->endts = proginfo->endts.addSecs(5 * 60);
            proginfo->spread++;
        }
    }

    return proginfo;
}

void GuideGrid::fillProgramInfos(void)
{
    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            if (m_programInfos[y][x])
                delete m_programInfos[y][x];
            m_programInfos[y][x] = NULL;
        }
    }

    for (int y = 0; y < DISPLAY_CHANS; y++)
    {
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            ProgramInfo *pginfo = getProgramInfo(y, x);
            if (!pginfo)
                pginfo = new ProgramInfo;
            m_programInfos[y][x] = pginfo;
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
        if (r.intersects(dateRect()))
            paintDate(&p);
        if (r.intersects(channelRect()))
            paintChannels(&p);
        if (r.intersects(timeRect()))
            paintTimes(&p);
        if (r.intersects(programRect()))
            paintPrograms(&p);
        if (showtitle && r.intersects(titleRect()))
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

    tmp.drawText((dr.width() - datewidth) / 2,
                 (dr.height() - dateheight) / 2 + dateheight / 3, date);

    date = m_currentStartTime.toString("MMM d");
    datewidth = lfm.width(date);

    tmp.drawText((dr.width() - datewidth) / 2,
                 (dr.height() - dateheight) / 2 + dateheight * 4 / 3, date);

    tmp.drawLine(0, dr.height() - 1, dr.right(), dr.height() - 1);
    tmp.drawLine(dr.width() - 1, 0, dr.width() - 1, dr.height());

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

    for (unsigned int y = 0; y < DISPLAY_CHANS; y++)
    {
        unsigned int chanNumber = y + m_currentStartChannel;
        if (chanNumber >= m_channelInfos.size())
            chanNumber -= m_channelInfos.size();
  
        if (m_channelInfos[chanNumber].chanstr == "")
            break;

        ChannelInfo *chinfo = &(m_channelInfos[chanNumber]);
        if (chinfo->iconpath != "none" && chinfo->iconpath != "")
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

        tmp.setFont(*m_chanFont);
        QFontMetrics lfm(*m_chanFont);
        int width = lfm.width(chinfo->chanstr);
        int bheight = lfm.height();
       
        int yoffset = (int)(43 * hmult); 
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

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x]->min % 30 == 0)
        {
            tmp.drawLine((x + 6) * xdifference, 0, (x + 6) * xdifference, 
                         tr.bottom());
        }
    }

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        if (m_timeInfos[x]->min % 30 == 0)
        {
            TimeInfo *tinfo = m_timeInfos[x];

            QFontMetrics fm(*m_timeFont);
            int width = fm.width(tinfo->usertime);
            int height = fm.height();

            int xpos = (x * xdifference) + (xdifference * 6 - width) / 2; 
            tmp.drawText(xpos, (tr.bottom() - height) / 2 + height, 
                         tinfo->usertime);
        }
    }

    tmp.drawLine(0, tr.height() - 1, tr.right(), tr.height() - 1);

    tmp.end();

    p->drawPixmap(tr.topLeft(), pix);
}

QBrush GuideGrid::getBGColor(const QString &category)
{
    QBrush br = QBrush(bgcolor);
   
    if (!usetheme)
        return br;

    QString cat = "Cat_" + category;

    QString color = m_settings->GetSetting(cat);
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

    for (int y = 1; y < DISPLAY_CHANS + 1; y++)
    {
        int ypos = ydifference * y;
        tmp.drawLine(0, ypos, pr.right(), ypos);
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
                if (pginfo->category != "" && usetheme)
                    info += " (" + pginfo->category + ")";
                
                int startx = (int)(x * xdifference + 7 * wmult);
                tmp.drawText(startx,
                             (int)(height / 8 + y * ydifference + 1 * hmult), 
                             maxwidth, ydifference - height / 8,
                             AlignLeft | WordBreak,
                             info);

                tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));

                startx = (int)((x + spread) * xdifference); 
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

                    startx = (int)((x + spread) * xdifference - width * 1.5); 
                    tmp.drawText(startx, (y + 1) * ydifference - height,
                                 (int)(width * 1.5), height, AlignLeft, text);
            
                    tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
                }

                if (m_currentRow == (int)y)
                {
                    if ((m_currentCol >= x) && (m_currentCol < (x + spread)))
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
                        int xend = (int)(xdifference * spread - 4 * tmpwmult);
                        if (x == 0)
                            xend += (int)(1 * tmpwmult);
			
                        tmp.drawRect(xstart, ystart, xend, rectheight);
                        tmp.setPen(QPen(fgcolor, (int)(2 * wmult)));
                    }
                }
            }
            lastprog = pginfo->startts;
        }
    }

    tmp.end();

    p->drawPixmap(pr.topLeft(), pix);
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
    QRect r(0, 0, programRect().left(), programRect().top());
    return r;
}

QRect GuideGrid::channelRect() const
{
    QRect r(0, dateRect().height(), dateRect().width(), programRect().height());
    return r;
}

QRect GuideGrid::timeRect() const
{
    QRect r(dateRect().width(), 0, programRect().width(), dateRect().height());
    return r;
}

QRect GuideGrid::programRect() const
{
    // Change only these numbers to adjust the size of the visible regions

    unsigned int min_dateheight = 50;  // also min_timeheight
    unsigned int min_datewidth = 74;   // also min_chanwidth
    unsigned int titleheight = showtitle ? 40 : 0;

    unsigned int programheight = (int)((600 - min_dateheight - titleheight) * 
                                       wmult);
    programheight = DISPLAY_CHANS * (int)(programheight / DISPLAY_CHANS);

    unsigned int programwidth = (int)((800 - min_datewidth) * hmult);
    programwidth = DISPLAY_TIMES * (int)(programwidth / DISPLAY_TIMES);

    QRect r((int)(800 * wmult) - programwidth, 
            (int)(600 * hmult) - programheight - titleheight,
            programwidth, programheight);

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

    update(programRect());
    if (showtitle)
        update(titleRect());
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

    update(programRect());
    if (showtitle)
        update(titleRect());
}

void GuideGrid::cursorDown()
{
    m_currentRow++;

    if (m_currentRow > DISPLAY_CHANS - 1)
    {
        m_currentRow = DISPLAY_CHANS - 1;
        emit scrollDown();
    }

    update(programRect());
    if (showtitle)
        update(titleRect());
}

void GuideGrid::cursorUp()
{
    m_currentRow--;

    if (m_currentRow < 0)
    {
        m_currentRow = 0;
        emit scrollUp();
    }

    update(programRect());
    if (showtitle)
        update(titleRect());
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

    for (int x = DISPLAY_TIMES - 6; x < DISPLAY_TIMES; x++)
    {
        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            ProgramInfo *pginfo = m_programInfos[y][x];
            if (pginfo)
                delete pginfo;
            m_programInfos[y][x] = NULL;
        }
    }

    for (int x = DISPLAY_TIMES - 1; x >= 6; x--)
    {
        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            m_programInfos[y][x] = m_programInfos[y][x - 6];
            m_programInfos[y][x]->spread = -1;
            m_programInfos[y][x]->startCol = x;
        }
    }

    for (int x = 0; x < 6; x++)
    {
        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            ProgramInfo *pginfo = getProgramInfo(y, x);
            m_programInfos[y][x] = pginfo;
        }
    }

    update(timeRect());
    if (updatedate) 
        update(dateRect());
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

    for (int x = 0; x < 6; x++)
    {
        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            ProgramInfo *pginfo = m_programInfos[y][x];
            if (pginfo)
                delete pginfo;
            m_programInfos[y][x] = NULL;
        }
    }

    for (int x = 0; x < DISPLAY_TIMES - 6; x++)
    {
        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            m_programInfos[y][x] = m_programInfos[y][x + 6];
            m_programInfos[y][x]->spread = -1;
            m_programInfos[y][x]->startCol = x;
        }
    }

    for (int x = DISPLAY_TIMES - 6; x < DISPLAY_TIMES; x++)
    {
        for (int y = 0; y < DISPLAY_CHANS; y++)
        {
            ProgramInfo *pginfo = getProgramInfo(y, x);
            m_programInfos[y][x] = pginfo;
        }
    }

    update(timeRect());
    if (updatedate)
        update(dateRect());
}

void GuideGrid::scrollDown()
{
    m_currentStartChannel++;
    if (m_currentStartChannel >= m_channelInfos.size())
        m_currentStartChannel -= m_channelInfos.size();

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = m_programInfos[0][x];
        if (pginfo)
            delete pginfo;
        m_programInfos[0][x] = NULL;
    }

    for (int y = 0; y < DISPLAY_CHANS - 1; y++)
    {
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            m_programInfos[y][x] = m_programInfos[y + 1][x];
        }
    } 

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = getProgramInfo(DISPLAY_CHANS - 1, x);
        m_programInfos[DISPLAY_CHANS - 1][x] = pginfo;
    }

    update(channelRect());
}

void GuideGrid::scrollUp()
{
    if (m_currentStartChannel == 0)
        m_currentStartChannel = m_channelInfos.size() - 1;
    else
        m_currentStartChannel--;

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = m_programInfos[DISPLAY_CHANS - 1][x];
        if (pginfo)
            delete pginfo;
        m_programInfos[DISPLAY_CHANS - 1][x] = NULL;
    }

    for (int y = DISPLAY_CHANS - 1; y > 0; y--)
    {
        for (int x = 0; x < DISPLAY_TIMES; x++)
        {
            m_programInfos[y][x] = m_programInfos[y - 1][x];
        }
    } 

    for (int x = 0; x < DISPLAY_TIMES; x++)
    {
        ProgramInfo *pginfo = getProgramInfo(0, x);
        m_programInfos[0][x] = pginfo;
    }

    update(channelRect());
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

    update(programRect());
}
