#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qtimer.h>

#include <unistd.h>

#include "singleview.h"

#include <mythtv/mythcontext.h>

SingleView::SingleView(MythContext *context, vector<Thumbnail> *imagelist, 
                       int pos, QWidget *parent, const char *name)
	  : QDialog(parent, name)
{
    m_context = context;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setCursor(QCursor(Qt::BlankCursor));

    context->ThemeWidget(this, screenwidth, screenheight, wmult, hmult);

    m_font = new QFont("Arial", (int)(context->GetSmallFontSize() * hmult), 
                       QFont::Bold);

    images = imagelist;
    imagepos = pos;
    displaypos = -1;

    image = NULL;

    timerrunning = false;
    timersecs = context->GetNumSetting("SlideshowDelay");
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

    delete m_font;
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
