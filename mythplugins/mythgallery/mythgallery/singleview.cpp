#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qtimer.h>

#include <unistd.h>

#include "singleview.h"

#include <mythtv/settings.h>

extern Settings *globalsettings;

SingleView::SingleView(vector<Thumbnail> *imagelist, int pos, QWidget *parent, 
                       const char *name)
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

    bool usetheme = globalsettings->GetNumSetting("ThemeQt");

    if (usetheme)
    {
        bgcolor = QColor(globalsettings->GetSetting("BackgroundColor"));
        fgcolor = QColor(globalsettings->GetSetting("ForegroundColor"));

        QPixmap *bgpixmap = NULL;

        if (globalsettings->GetSetting("BackgroundPixmap") != "")
        {
            QString pmapname = globalsettings->GetSetting("ThemePathName") +
                               globalsettings->GetSetting("BackgroundPixmap");

            bgpixmap = loadScalePixmap(pmapname, screenwidth, screenheight,
                                       wmult, hmult);
            setPaletteBackgroundPixmap(*bgpixmap);
        }
        else if (globalsettings->GetSetting("TiledBackgroundPixmap") != "")
        { 
            QString pmapname = globalsettings->GetSetting("ThemePathName") +
                          globalsettings->GetSetting("TiledBackgroundPixmap");

            bgpixmap = loadScalePixmap(pmapname, screenwidth, screenheight,
                                       wmult, hmult);
    
            QPixmap background(screenwidth, screenheight);
            QPainter tmp(&background);

            tmp.drawTiledPixmap(0, 0, screenwidth, screenheight, *bgpixmap);
            tmp.end();
            setPaletteBackgroundPixmap(background);
        }
        else
            setPalette(QPalette(bgcolor));

        if (bgpixmap)
            delete bgpixmap;
    }   
    else
    {       
        bgcolor = QColor("white");
        fgcolor = QColor("black");
        setPalette(QPalette(bgcolor));
    }

    m_font = new QFont("Arial", (int)(13 * hmult), QFont::Bold);

    images = imagelist;
    imagepos = pos;
    displaypos = -1;

    image = NULL;

    timerrunning = false;
    timersecs = globalsettings->GetNumSetting("SlideshowDelay");
    if (!timersecs)
        timersecs = 5;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), SLOT(advanceFrame()));    

    WFlags flags = getWFlags();
    setWFlags(flags | Qt::WRepaintNoErase);

    showFullScreen();
}

SingleView::~SingleView()
{
    if (image)
        delete image;
    if (timer)
    {
        timer->stop(); 
        while (timer->isActive())
            usleep(50);
        delete timer;
    }
}

void SingleView::paintEvent(QPaintEvent *e)
{
    e = e;

    erase();

    QPainter p(this);

    if (displaypos != imagepos)
    {
        QString filename = (*images)[imagepos].filename;

        QImage tmpimage(filename);
        QImage tmp2 = tmpimage.smoothScale(screenwidth, screenheight,
                                           QImage::ScaleMin);

        if (image)
            delete image;

        image = new QImage(tmp2);
    
        displaypos = imagepos;
    }

    p.drawImage((screenwidth - image->width()) / 2, 
                (screenheight - image->height()) / 2, *image);
}

void SingleView::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    int oldpos = imagepos;
    timer->stop();

    switch (e->key())
    {
        case Key_Up:
        case Key_Left:
        {
            imagepos--;
            if (imagepos < 0)
                imagepos = images->size() - 1;
            handled = true;
            break;
        }
        case Key_Right:
        case Key_Down:
        case Key_Space:
        case Key_Enter:
        case Key_Return:
        {
            imagepos++;
            if (imagepos == (int)images->size())
                imagepos = 0;
            handled = true;
            break;
        }
        case 'P':
        {
            timerrunning = !timerrunning;
            handled = true;
            break;
        }
        default: break;
    }

    if (handled)
    {
        update();
        if (timerrunning)
            timer->start(timersecs * 1000);
    }
    else
    {
        imagepos = oldpos;
        QDialog::keyPressEvent(e);
    }
}

void SingleView::advanceFrame(void)
{
    imagepos++;
    if (imagepos == (int)images->size())
        imagepos = 0;

    update();
}
