#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qtimer.h>
#include <qfileinfo.h>

#include <unistd.h>

#include "singleview.h"

#include <mythtv/mythcontext.h>
#include <mythtv/dialogbox.h>

SingleView::SingleView(QSqlDatabase *db, vector<Thumbnail> *imagelist, int pos,
                       MythMainWindow *parent, const char *name)
	  : MythDialog(parent, name)
{
    m_db = db;
    redraw = false;
    rotateAngle = 0; 
    imageRotateAngle = 0;
    zoomfactor = newzoom = 0;

    m_font = new QFont("Arial", (int)(gContext->GetMediumFontSize() * hmult), 
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

    setNoErase();
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
    QString bgtype = gContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette (QColor(bgtype)));

    QPainter p(this);

    if (displaypos != imagepos || zoomfactor != newzoom || redraw)
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
        origWidth = tmpimage.width();
        origHeight = tmpimage.height();
        QWMatrix matrix;
        matrix.rotate(rotateAngle);
        QImage tmp1 = tmpimage.xForm(matrix);
        QImage tmp2;
 
        if (newzoom > 1)
            tmp2 = tmp1.smoothScale(screenwidth * newzoom,
                                    screenheight * newzoom,
                                    QImage::ScaleMax);
        else if (newzoom)
            tmp2 = tmp1.smoothScale(screenwidth, screenheight,
                                    QImage::ScaleMax);
        else 
            tmp2 = tmp1.smoothScale(screenwidth, screenheight,
                                    QImage::ScaleMin);

        zoomed_h = tmp2.height();
        zoomed_w = tmp2.width();

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

    if (newzoom)
    {
        if (!zoomfactor)
        {
            sx = (zoomed_w - screenwidth) / 2;
            sy = (zoomed_h - screenheight) / 2;
        } 
        else if (newzoom != zoomfactor)
        {
            sx = ((newzoom * (sx + (screenwidth / 2))) / zoomfactor) - 
                 (screenwidth / 2);
            sy = ((newzoom * (sy + (screenheight / 2))) / zoomfactor) - 
                 (screenheight / 2);
        }
        // boundary checks
        sx = (sx > (zoomed_w - screenwidth)) ? (zoomed_w - screenwidth) : sx;
        sy = (sy > (zoomed_h - screenheight)) ? (zoomed_h - screenheight) : sy;
        sx = (sx < 0) ? 0 : sx;
        sy = (sy < 0) ? 0 : sy;

        p.drawImage(0, 0, *image, sx, sy, screenwidth, screenheight);

        QString zoomstring;
        if (newzoom > 1)
            zoomstring = QString("%1x Zoom").arg(newzoom);
        else if (screenheight != zoomed_h)
            zoomstring = "Screenwidth Zoom";
        else if (screenwidth != zoomed_w)
            zoomstring = "Screenheight Zoom";
        else
            zoomstring = "1x Zoom";
        p.drawText(screenwidth / 10, screenheight / 10, zoomstring);
        zoomfactor = newzoom;
    } else
    {
        p.drawImage((screenwidth - image->width()) / 2, 
                    (screenheight - image->height()) / 2, *image);
        sx = sy = 0;
    }
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
            newzoom = 0;
            break;
        }
        case '[':
        {
            rotateAngle -= 90;
            handled = true;
            redraw = true;
            newzoom = 0;
            break;
        }
        case Key_Left:
        case Key_Up:
        {
            retreatFrame(false);
            handled = true;
            rotateAngle = 0;
            break;
        }
        case Key_I:
        {
            handled=true;
            QString filename = (*images)[imagepos].filename;
            QFileInfo fi(filename);
            QString info((*images)[imagepos].name);
            info += "\n\nFilename: " + filename;
            info += "\nCreated: " + fi.created().toString();
            info += "\nModified: " + fi.lastModified().toString();
            info += QString("\nBytes: %1").arg(fi.size());
            info += QString("\nWidth: %1 pixels").arg(origWidth);
            info += QString("\nHeight: %1 pixels").arg(origHeight);
            info += QString("\nPixel Count: %1 megapixels")
                            .arg((float)origHeight * origWidth / 1000000, 0, 
                                  'f', 2);
            info += QString("\nRotation Angle: %1 degrees").arg(rotateAngle);
            DialogBox fiDlg(gContext->GetMainWindow(), info);
            fiDlg.AddButton(tr("OK"));
            fiDlg.exec();
            break;
        }
        case Key_7: // zoom out
        {
            handled = zoomfactor;
            newzoom = (zoomfactor < 2) ? zoomfactor : (zoomfactor - 1);
            break;
        }
        case Key_9: // zoom in
        {
            handled = true;
            newzoom = zoomfactor ? (zoomfactor + 1) : 2;
            timerrunning = false;
            break;
        }
        case Key_2: // scroll up
        {
            handled = zoomfactor;
            sy -= screenheight/2;
            sy = (sy < 0) ? 0 : sy;
            break;
        }
        case Key_4: // scroll left
        {
            handled = zoomfactor;
            sx -= screenwidth/2;
            sx = (sx < 0) ? 0 : sx;
            break;
        }
        case Key_6: // scroll right
        {
            handled=zoomfactor;
            sx += screenwidth/2;
            sx = (sx > (zoomed_w-screenwidth)) ? (zoomed_w - screenwidth) : sx;
            break;
        }
        case Key_8: // scroll down
        {
            handled = zoomfactor;
            sy += screenheight/2;
            sy = (sy>(zoomed_h-screenheight)) ? (zoomed_h - screenheight):sy;
            break;
        }
        case Key_5: // recenter
        {
            handled = zoomfactor;
            sx = (zoomed_w - screenwidth)/2;
            sy = (zoomed_h - screenheight)/2;
            break;
        }
        case Key_0: // fullsize 
        {
            handled = zoomfactor;
            newzoom = 0;
            break;
        }
        case Key_PageUp: // upper left corner
        {
            handled = zoomfactor;
            sx = sy = 0;
            break;
        }
        case Key_PageDown: // lower right corner 
        {
            handled = zoomfactor;
            sx = zoomed_w - screenwidth;
            sy = zoomed_h - screenheight;
            break;
        }
        case Key_Right:
        case Key_Down:
        case Key_Space:
        case Key_Enter:
        case Key_Return:
        {
            advanceFrame(false);
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
        MythDialog::keyPressEvent(e);
    }
}

void SingleView::retreatFrame(bool doUpdate)
{
    imagepos--;
    if (imagepos < 0)
        imagepos = images->size() - 1;

    if ((*images)[imagepos].isdir)
        retreatFrame(doUpdate);

    newzoom = 0;
    if (doUpdate)
        update();
}

void SingleView::advanceFrame(bool doUpdate)
{
    imagepos++;
    if (imagepos == (int)images->size())
        imagepos = 0;

    if ((*images)[imagepos].isdir)
        advanceFrame(doUpdate);

    newzoom = 0;
    if (doUpdate)
        update();
}

void SingleView::startShow(void)
{
    timerrunning = true;
    timer->start(timersecs * 1000);
}

