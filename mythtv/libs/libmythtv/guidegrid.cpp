#include <qapplication.h>
#include <qpainter.h>
#include <qfont.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qaccel.h>
#include <math.h>
#include <qcursor.h>
#include <qapplication.h>

#include <iostream>
using namespace std;

#include "guidegrid.h"
#include "infodialog.h"
#include "infostructs.h"
#include "libmyth/programinfo.h"
#include "libmyth/settings.h"

extern Settings *globalsettings;

GuideGrid::GuideGrid(const QString &channel, QWidget *parent, const char *name)
         : QDialog(parent, name)
{
    screenheight = QApplication::desktop()->height();
    screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    wmult = screenwidth / 800.0;
    hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setCursor(QCursor(Qt::BlankCursor));

    usetheme = globalsettings->GetNumSetting("ThemeQt");

    if (usetheme)
    {
        bgcolor = QColor(globalsettings->GetSetting("BackgroundColor"));
        fgcolor = QColor(globalsettings->GetSetting("ForegroundColor"));
    }
    else
    {
        bgcolor = QColor("white");
        fgcolor = QColor("black");
    }

    setPalette(QPalette(bgcolor));
    m_font = new QFont("Arial", 11 * hmult, QFont::Bold);
    m_largerFont = new QFont("Arial", 13 * hmult, QFont::Bold);

    m_originalStartTime = QDateTime::currentDateTime();
    m_currentStartTime = m_originalStartTime;
    m_currentStartChannel = 0;
    m_startChanStr = channel;

    m_currentRow = 0;
    m_currentCol = 0;

    for (int y = 0; y < 10; y++)
    {
        m_timeInfos[y] = NULL;
        for (int x = 0; x < 10; x++)
            m_programInfos[x][y] = NULL;
    }

    QTime clock = QTime::currentTime();
    clock.start();
    fillTimeInfos();
    int filltime = clock.elapsed();
    clock.restart();
    fillChannelInfos();
    int fillchannels = clock.elapsed();
    clock.restart();
    fillProgramInfos();
    int fillprogs = clock.elapsed();

    cout << filltime << " " << fillchannels << " " << fillprogs << endl;

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

    showFullScreen();
    setActiveWindow();
    raise();
    setFocus();
}

GuideGrid::~GuideGrid()
{
    for (int y = 0; y < 10; y++)
    {
        if (m_timeInfos[y])
            delete m_timeInfos[y];

        for (int x = 0; x < 10; x++)
        {
            if (m_programInfos[x][y])
                delete m_programInfos[x][y];
        }
    }

    m_channelInfos.clear();
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
    
    thequery = "SELECT channum,callsign,icon FROM channel ORDER BY channum;";
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
            val.chanstr = query.value(0).toString();
            val.icon = NULL;
        
            if (val.chanstr == m_startChanStr && !set)
            {
                m_currentStartChannel = m_channelInfos.size();
                set = true;
            }
		
            m_channelInfos.push_back(val);
        }
    }
}

void GuideGrid::fillTimeInfos()
{
    for (int x = 0; x < 10; x++)
    {
        if (m_timeInfos[x])
            delete m_timeInfos[x];
        m_timeInfos[x] = NULL;
    }

    QDateTime t = m_currentStartTime;

    char temp[512];
    for (int x = 0; x < 6; x++)
    {
        TimeInfo *timeinfo = new TimeInfo;

        int year = t.date().year();
        int month = t.date().month();
        int day = t.date().day();

        int hour = t.time().hour();
        int mins = t.time().minute();

        int sqlmins;        
        if (mins >= 30)
        {
            mins = 30;
            sqlmins = 45;
        }
        else
        {
            sqlmins = 15;
            mins = 0;
        }

        bool am = true;
        if (hour >= 12)
            am = false;

        if (hour == 0)
            sprintf(temp, "12:%02d AM", mins);
        else
        {
            if (hour > 12)
                sprintf(temp, "%02d:%02d ", hour - 12, mins);
            else 
                sprintf(temp, "%02d:%02d ", hour, mins);
            strcat(temp, (am ? "AM" : "PM"));
        }
        timeinfo->year = year;
        timeinfo->month = month;
        timeinfo->day = day;
        timeinfo->hour = hour;
        timeinfo->min = mins;

        timeinfo->usertime = temp;

        sprintf(temp, "%4d%02d%02d%02d%02d50", year, month, day, hour, sqlmins);
        timeinfo->sqltime = temp;

        m_timeInfos[x] = timeinfo;

        t = t.addSecs(1800);
    }
}

ProgramInfo *GuideGrid::getProgramInfo(unsigned int row, unsigned int col)
{
    unsigned int chanNum = row + m_currentStartChannel;
    if (chanNum >= m_channelInfos.size())
        chanNum -= m_channelInfos.size();

    if (m_channelInfos[chanNum].chanstr == "" || !m_timeInfos[col])
        return NULL;

    ProgramInfo *pginfo = GetProgramAtDateTime(m_channelInfos[chanNum].chanstr,
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
    proginfo->startts = ts;
    if (col < 5)
    { 
        tinfo = m_timeInfos[col + 1];
        ts.setDate(QDate(tinfo->year, tinfo->month, tinfo->day));
        ts.setTime(QTime(tinfo->hour, tinfo->min));
    }
    proginfo->endts = ts;
    proginfo->spread = 1;
    proginfo->startCol = col;
    return proginfo;
}

void GuideGrid::fillProgramInfos(void)
{
    for (int y = 0; y < 10; y++)
    {
        for (int x = 0; x < 10; x++)
        {
            if (m_programInfos[y][x])
                delete m_programInfos[y][x];
            m_programInfos[y][x] = NULL;
        }
    }

    for (int y = 0; y < 6; y++)
    {
        for (int x = 0; x < 5; x++)
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
        if (r.intersects(programRect()))
            paintPrograms(&p);  
        if (r.intersects(channelRect()))
            paintChannels(&p);
        if (r.intersects(timeRect()))
            paintTimes(&p);
    }
}

void GuideGrid::paintChannels(QPainter *p)
{
    QRect cr = channelRect();
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, 2 * wmult));
    tmp.setFont(*m_largerFont);

    QString date = m_currentStartTime.toString("ddd");
    QFontMetrics lfm(*m_largerFont);
    int datewidth = lfm.width(date);
    int dateheight = lfm.height();

    tmp.drawText((cr.width() - datewidth) / 2, (55 * hmult - dateheight) / 2,
                 date);

    date = m_currentStartTime.toString("MMM d");
    datewidth = lfm.width(date);

    tmp.drawText((cr.width() - datewidth) / 2, 
                 (55 * hmult - dateheight) / 2 + dateheight, date);

    tmp.setFont(*m_font);

    int ydifference = (int)(ceil((600 * hmult - 49 * hmult) / 6));
    int yoffset = (int)(49 * hmult);
    for (unsigned int i = 0; i < 6; i++)
    {
        tmp.drawLine(0, yoffset + ydifference * i, cr.right(), 
                     yoffset + ydifference * i);
    }
    tmp.drawLine(cr.right(), 0, cr.right(), cr.height());

    for (unsigned int i = 0; i < 6; i++)
    {
        unsigned int chanNumber = i + m_currentStartChannel;
        if (chanNumber >= m_channelInfos.size())
            chanNumber -= m_channelInfos.size();
  
        if (m_channelInfos[chanNumber].chanstr == "")
            break;

        ChannelInfo *chinfo = &(m_channelInfos[chanNumber]);
        if (chinfo->iconpath != "none")
        {
            if (!chinfo->icon)
                chinfo->LoadIcon();
            yoffset = (int)(55 * hmult);
            tmp.drawPixmap((cr.width() - chinfo->icon->width()) / 2, 
                           ydifference * i + yoffset, *(chinfo->icon));
        }
        tmp.setFont(*m_largerFont);
        QFontMetrics lfm(*m_largerFont);
        int width = lfm.width(chinfo->chanstr);
        int bheight = lfm.height();
            
        yoffset = (int)(85 * hmult);
        tmp.drawText((cr.width() - width) / 2, 
                     ydifference * i + yoffset + bheight, chinfo->chanstr);

        tmp.setFont(*m_font);
        QFontMetrics fm(*m_font);
        width = fm.width(chinfo->callsign);
        int height = fm.height();
        tmp.drawText((cr.width() - width) / 2, 
                     ydifference * i + yoffset + bheight + height,
                     chinfo->callsign);
    }

    tmp.end();
    
    p->drawPixmap(cr.topLeft(), pix);
}

void GuideGrid::paintTimes(QPainter *p)
{
    QRect cr = timeRect();
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp(&pix);
    tmp.setBrush(fgcolor);
    tmp.setPen(QPen(fgcolor, 2 * wmult));
    tmp.setFont(*m_largerFont);

    int xdifference = (int)(cr.right() / 6);

    for (int i = 1; i < 6; i++)
    {
        tmp.drawLine(i * xdifference, 0, i * xdifference, cr.bottom());
    }

    for (int i = 0; i < 5; i++)
    {
        TimeInfo *tinfo = m_timeInfos[i];

        QFontMetrics fm(*m_largerFont);
        int width = fm.width(tinfo->usertime);
        int height = fm.height();

        tmp.drawText((xdifference - width) / 2 + i * xdifference, 
                     (cr.bottom() - height) / 2 + height, 
                     tinfo->usertime);
    }

    tmp.drawLine(0, cr.bottom(), cr.right(), cr.bottom());

    tmp.end();

    p->drawPixmap(cr.topLeft(), pix);
}

QBrush GuideGrid::getBGColor(const QString &category)
{
    QBrush br = QBrush(bgcolor);
   
    if (!usetheme)
        return br;

    QString cat = "Cat_" + category;

    QString color = globalsettings->GetSetting(cat);
    if (color != "")
    {
        br = QBrush(color);
    }

    return br;
}

void GuideGrid::paintPrograms(QPainter *p)
{
    QRect cr = programRect();
    QPixmap pix(cr.size());
    pix.fill(this, cr.topLeft());

    QPainter tmp(&pix);
    tmp.setPen(QPen(fgcolor, 2 * wmult));

    tmp.setFont(*m_largerFont);

    int ydifference = (int)(ceil((600 * hmult - 49 * hmult) / 6));
    int xdifference = (int)(cr.right() / 6);

    for (int i = 1; i < 6; i++)
    {
        int ypos = ydifference * i;
        tmp.drawLine(0, ypos, cr.right(), ypos);
    }

    for (unsigned int y = 0; y < 6; y++)
    {
        unsigned int chanNum = y + m_currentStartChannel;
        if (chanNum >= m_channelInfos.size())
            chanNum -= m_channelInfos.size();

        if (m_channelInfos[chanNum].chanstr == "")
            break;

        QDateTime lastprog;
        for (int x = 0; x < 5; x++)
        {
            ProgramInfo *pginfo = m_programInfos[y][x];

            if (pginfo->recordtype == -1)
                pginfo->GetProgramRecordingStatus();

            int spread = 1;
            if (pginfo->startts != lastprog)
            {
                QFontMetrics fm(*m_largerFont);
                int height = fm.height();

                if (pginfo->spread != -1)
                {
                    spread = pginfo->spread;
                }
                else
                {
                    for (int z = x + 1; z < 5; z++)
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

                tmp.fillRect(x * xdifference + 1 * wmult, 
                             ydifference * y + 1 * hmult,
                             (x + spread) * xdifference - 2 * wmult, 
                             ydifference - 2 * hmult, br);

                int maxwidth = (int)(spread * xdifference - (15 * wmult));

                QString info = pginfo->title;
                if (pginfo->category != "" && usetheme)
                    info += " (" + pginfo->category + ")";
                
                tmp.drawText(x * xdifference + 10 * wmult, 
                             height / 8 + y * ydifference + 1 * hmult, 
                             maxwidth, ydifference,
                             AlignLeft | WordBreak,
                             info);

                tmp.setPen(QPen(fgcolor, 2 * wmult));

                tmp.drawLine((x + spread) * xdifference, ydifference * y, 
                             (x + spread) * xdifference, 
                             ydifference * (y + 1));

                if (pginfo->recordtype > 0)
                {
                    tmp.setPen(QPen(red, 2 * wmult));
                    QString text;

                    if (pginfo->recordtype == 1)
                        text = "R";
                    else if (pginfo->recordtype == 2)
                        text = "T";
                    else if (pginfo->recordtype == 3) 
                        text = "A";

                    int width = fm.width(text);

                    tmp.drawText((x + spread) * xdifference - width * 1.5, 
                                 (y + 1) * ydifference - height, width * 1.5, 
                                 height, AlignLeft, text);
             
                    tmp.setPen(QPen(fgcolor, 2 * wmult));
                }

                if (m_currentRow == (int)y)
                {
                    if ((m_currentCol >= x) && (m_currentCol < (x + spread)))
                    {
                        tmp.setPen(QPen(red, 3.75 * wmult));
                  
                        int rectheight = (int)(ydifference - 4 * hmult);
                        int xstart = 1;
                        if (x != 0)
                            xstart = (int)(x * xdifference + 2 * wmult);
                        int xend = (int)(xdifference * spread - 4 * wmult);
                        if (x == 0)
                            xend += (int)(1 * wmult);
			
                        tmp.drawRect(xstart, 
                                     ydifference * y + 2 * hmult, 
                                     xend, rectheight); 
                        tmp.setPen(QPen(fgcolor, 2 * wmult));
                    }
                }
            }
            lastprog = pginfo->startts;
        }
    }

    tmp.end();

    p->drawPixmap(cr.topLeft(), pix);
}

QRect GuideGrid::fullRect() const
{
    QRect r(0, 0, 800 * wmult, 600 * hmult);
    return r;
}

QRect GuideGrid::channelRect() const
{
    QRect r(0, 0, 74 * wmult, 600 * hmult);
    return r;
}

QRect GuideGrid::timeRect() const
{
    QRect r(74 * wmult, 0, 800 * wmult, 49 * hmult + 1);
    return r;
}

QRect GuideGrid::programRect() const
{
    QRect r(74 * wmult, 49 * hmult, 800 * wmult, 600 * hmult);
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
        update(programRect());
}

void GuideGrid::cursorRight()
{
    int spread = m_programInfos[m_currentRow][m_currentCol]->spread;
    int startCol = m_programInfos[m_currentRow][m_currentCol]->startCol;

    m_currentCol = startCol + spread;

    if (m_currentCol > 4)
    {
        m_currentCol = 4;
        emit scrollRight();
    }
    else
        update(programRect());
}

void GuideGrid::cursorDown()
{
    m_currentRow++;

    if (m_currentRow > 5)
    {
        m_currentRow = 5;
        emit scrollDown();
    }
    else
        update(programRect());
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
        update(programRect());
}

void GuideGrid::scrollLeft()
{
    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(-1800);

    bool updatechannel = false;
    if (t.date().day() != m_currentStartTime.date().day())
        updatechannel = true;

    m_currentStartTime = t;

    fillTimeInfos();

    for (int x = 0; x < 6; x++)
    {
        ProgramInfo *pginfo = m_programInfos[x][4];
        if (pginfo)
            delete pginfo;
        m_programInfos[x][4] = NULL;
    }

    for (int y = 4; y > 0; y--)
    {
        for (int x = 0; x < 6; x++)
        {
            m_programInfos[x][y] = m_programInfos[x][y - 1];
            m_programInfos[x][y]->spread = -1;
            m_programInfos[x][y]->startCol = y;
        }
    }

    for (int x = 0; x < 6; x++)
    {
        ProgramInfo *pginfo = getProgramInfo(x, 0);
        m_programInfos[x][0] = pginfo;
    }

    update(programRect().unite(timeRect()));
    if (updatechannel) 
        update(channelRect());
}

void GuideGrid::scrollRight()
{
    QDateTime t = m_currentStartTime;

    t = m_currentStartTime.addSecs(1800);

    bool updatechannel = false;
    if (t.date().day() != m_currentStartTime.date().day())
        updatechannel = true;

    m_currentStartTime = t;

    fillTimeInfos();

    for (int x = 0; x < 6; x++)
    {
        ProgramInfo *pginfo = m_programInfos[x][0];
        if (pginfo)
            delete pginfo;
        m_programInfos[x][0] = NULL;
    }

    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 6; x++)
        {
            m_programInfos[x][y] = m_programInfos[x][y + 1];
            m_programInfos[x][y]->spread = -1;
            m_programInfos[x][y]->startCol = y;
        }
    }

    for (int x = 0; x < 6; x++)
    {
        ProgramInfo *pginfo = getProgramInfo(x, 4);
        m_programInfos[x][4] = pginfo;
    }

    update(programRect().unite(timeRect()));
    if (updatechannel)
        update(channelRect());
}

void GuideGrid::scrollDown()
{
    m_currentStartChannel++;
    if (m_currentStartChannel >= m_channelInfos.size())
        m_currentStartChannel -= m_channelInfos.size();

    for (int y = 0; y < 5; y++)
    {
        ProgramInfo *pginfo = m_programInfos[0][y];
        if (pginfo)
            delete pginfo;
        m_programInfos[0][y] = NULL;
    }

    for (int x = 0; x < 5; x++)
    {
        for (int y = 0; y < 5; y++)
        {
            m_programInfos[x][y] = m_programInfos[x + 1][y];
        }
    } 

    for (int y = 0; y < 5; y++)
    {
        ProgramInfo *pginfo = getProgramInfo(5, y);
        m_programInfos[5][y] = pginfo;
    }

    update(programRect().unite(channelRect()));
}

void GuideGrid::scrollUp()
{
    if (m_currentStartChannel == 0)
        m_currentStartChannel = m_channelInfos.size() - 1;
    else
        m_currentStartChannel--;

    for (int y = 0; y < 5; y++)
    {
        ProgramInfo *pginfo = m_programInfos[5][y];
        if (pginfo)
            delete pginfo;
        m_programInfos[5][y] = NULL;
    }

    for (int x = 5; x > 0; x--)
    {
        for (int y = 0; y < 5; y++)
        {
            m_programInfos[x][y] = m_programInfos[x - 1][y];
        }
    } 

    for (int y = 0; y < 5; y++)
    {
        ProgramInfo *pginfo = getProgramInfo(0, y);
        m_programInfos[0][y] = pginfo;
    }

    update(programRect().unite(channelRect()));
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
        InfoDialog diag(pginfo, this, "Program Info");
        diag.setCaption("BLAH!!!");
        diag.exec();
    }

    showInfo = 0;

    setActiveWindow();
    setFocus();

    update(programRect());
}
