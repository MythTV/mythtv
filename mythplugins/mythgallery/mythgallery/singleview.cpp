#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qtimer.h>

#include <unistd.h>

#include "singleview.h"

#include <mythtv/mythcontext.h>

SingleView::SingleView(QSqlDatabase *db, vector<Thumbnail> *imagelist, int pos,
                       QWidget *parent, const char *name)
	  : MythDialog(parent, name)
{
    m_db = db;
    redraw = false;
    rotateAngle = 0; 
    imageRotateAngle = 0;

    m_font = new QFont("Arial", (int)(gContext->GetSmallFontSize() * hmult), 
                       QFont::Bold);

    images = imagelist;
    imagepos = pos;
    displaypos = -1;

    image = NULL;

    timerrunning = false;
    timersecs = gContext->GetNumSetting("SlideshowDelay");
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

    if (displaypos != imagepos || redraw)
    {
        QString filename = (*images)[imagepos].filename;

        // retrieve metadata for new image
        if (displaypos != imagepos) 
        {
            QString querystr = "SELECT angle FROM gallerymetadata WHERE "
                               "image=\"" + filename + "\";";
            QSqlQuery query = m_db->exec(querystr);
			
            if (query.isActive()  && query.numRowsAffected() > 0) 
            {
                query.next();
                imageRotateAngle = rotateAngle = query.value(0).toInt();
            }
        }

        QImage tmpimage(filename);
        QWMatrix matrix;
        matrix.rotate(rotateAngle);
        QImage tmp1 = tmpimage.xForm(matrix);

        QImage tmp2 = tmp1.smoothScale(screenwidth, screenheight,
                                           QImage::ScaleMin);

        if (image)
            delete image;

        image = new QImage(tmp2);
    
        displaypos = imagepos;

        // save settings for image
        QString querystr = "REPLACE INTO gallerymetadata SET image=\"" +
                           filename + "\", angle=" + 
                           QString::number(rotateAngle) + ";";
        QSqlQuery query = m_db->exec(querystr);

        // invalidate thumbnail for this image
        if (imageRotateAngle != rotateAngle) 
        {
            delete (*images)[imagepos].pixmap;
            (*images)[imagepos].pixmap = 0;
        }
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
        case ']':
        {
            rotateAngle += 90;
            handled = true;
            redraw = true;
            break;
        }
        case '[':
        {
            rotateAngle -= 90;
            handled = true;
            redraw = true;
            break;
        }
        case Key_Left:
        case Key_Up:
        {
            imagepos--;
            if (imagepos < 0)
                imagepos = images->size() - 1;
            handled = true;
            rotateAngle = 0;
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
            rotateAngle = 0;
            break;
        }
        case 'P':
        {
            timerrunning = !timerrunning;
            handled = true;
            rotateAngle = 0;
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
