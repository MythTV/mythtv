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
 
        if (newzoom)
            tmp2 = tmp1.smoothScale(screenwidth * newzoom,
                                    screenheight * newzoom,
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

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Gallery", e, actions);

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ROTRIGHT")
        {
            rotateAngle += 90;
            redraw = true;
            newzoom = 1;
        }
        else if (action == "ROTLEFT")
        {
            rotateAngle -= 90;
            redraw = true;
            newzoom = 1;
        }
        else if (action == "LEFT" || action == "UP")
        {
            retreatFrame(false);
            rotateAngle = 0;
        }
        else if (action == "INFO")
        {
            QString filename = (*images)[imagepos].filename;
            QFileInfo fi(filename);
            QString info((*images)[imagepos].name);
            info += "\n\n" + tr("Filename: ") + filename;
            info += "\n" + tr("Created: ") + fi.created().toString();
            info += "\n" + tr("Modified: ") + fi.lastModified().toString();
            info += "\n" + QString(tr("Bytes") + ": %1").arg(fi.size());
            info += "\n" + QString(tr("Width") + ": %1 " + tr("pixels"))
                                   .arg(origWidth);
            info += "\n" + QString(tr("Height") + ": %1 " + tr("pixels"))
                                   .arg(origHeight);
            info += "\n" + QString(tr("Pixel Count") + ": %1 " + 
                                   tr("megapixels"))
                                   .arg((float)origHeight * origWidth / 1000000,
                                   0, 'f', 2);
            info += "\n" + QString(tr("Rotation Angle") + ": %1 " +
                                   tr("degrees")).arg(rotateAngle);
            DialogBox fiDlg(gContext->GetMainWindow(), info);
            fiDlg.AddButton(tr("OK"));
            fiDlg.exec();
        }
        else if (action == "ZOOMOUT")
        {
            handled = zoomfactor;
            newzoom = (zoomfactor < 1) ? zoomfactor : (zoomfactor / 2);
        }
        else if (action == "ZOOMIN")
        {
            newzoom = zoomfactor ? (zoomfactor * 2) : 2;
            timerrunning = false;
        }
        else if (action == "SCROLLUP")
        {
            handled = zoomfactor;
            sy -= screenheight/2;
            sy = (sy < 0) ? 0 : sy;
        }
        else if (action == "SCROLLLEFT")
        {
            handled = zoomfactor;
            sx -= screenwidth/2;
            sx = (sx < 0) ? 0 : sx;
        }
        else if (action == "SCROLLRIGHT")
        {
            handled = zoomfactor;
            sx += screenwidth/2;
            sx = (sx > (zoomed_w-screenwidth)) ? (zoomed_w - screenwidth) : sx;
        }
        else if (action == "SCROLLDOWN")
        {
            handled = zoomfactor;
            sy += screenheight/2;
            sy = (sy>(zoomed_h-screenheight)) ? (zoomed_h - screenheight):sy;
        }
        else if (action == "RECENTER")
        {
            handled = zoomfactor;
            sx = (zoomed_w - screenwidth)/2;
            sy = (zoomed_h - screenheight)/2;
        }
        else if (action == "FULLSIZE")
        {
            handled = zoomfactor;
            newzoom = 1;
        }
        else if (action == "UPLEFT")
        {
            handled = zoomfactor;
            sx = sy = 0;
        }
        else if (action == "LOWRIGHT")
        {
            handled = zoomfactor;
            sx = zoomed_w - screenwidth;
            sy = zoomed_h - screenheight;
        }
        else if (action == "RIGHT" || action == "DOWN" || action == "SELECT")
        {
            advanceFrame(false);
            rotateAngle = 0;
        }
        else if (action == "PLAY")
        {
            timerrunning = !timerrunning;
            rotateAngle = 0;
        }
        else
            handled = false;
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

    if (imagepos == displaypos) // oops, we wrapped
        return;

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

