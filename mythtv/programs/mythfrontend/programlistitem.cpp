#include <qimage.h>
#include <qpixmap.h>
#include <qapplication.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "tv.h"
#include "remoteutil.h"
#include "programlistitem.h"

#include "libmyth/mythcontext.h"

ProgramListItem::ProgramListItem(QListView *parent, ProgramInfo *lpginfo, 
                                 int type)
               : QListViewItem(parent)
{
    pginfo = lpginfo;
    pixmap = NULL;
  
    QString dateformat = gContext->GetSetting("DateFormat", "ddd MMMM d");
    QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");
 
    if (type == 0 || type == 1)
    { 
        setText(0, pginfo->startts.toString(dateformat + " " + timeformat));
        setText(1, pginfo->title);

        if (type == 1)
        {
            long long size = pginfo->filesize;
            long int mbytes = size / 1024 / 1024;
            QString filesize = QString("%1 MB").arg(mbytes);

            setText(2, filesize);
        }
 
        QDateTime curtime = QDateTime::currentDateTime();

        if (curtime < pginfo->endts && curtime > pginfo->startts)
            pginfo->conflicting = true;
    }
    else
    {
        if (gContext->GetNumSetting("DisplayChanNum") != 0)
            setText(0, pginfo->chansign);
        else
            setText(0, pginfo->chanstr);
        setText(1, pginfo->startts.toString(dateformat + " " + timeformat));
        setText(2, pginfo->title);
        //setText(3, pginfo->recordingprofilename);
    }
}

QPixmap ProgramListItem::getPixmap(void)
{
    if (gContext->GetNumSetting("GeneratePreviewPixmaps") != 1)
        return QPixmap();

    if (pixmap)
        return *pixmap;

    QString filename = pginfo->pathname;
    filename += ".png";

    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    pixmap = gContext->LoadScalePixmap(filename);
    if (pixmap)
        return *pixmap;

    QImage *image = gContext->CacheRemotePixmap(filename);

    if (!image)
    {
        RemoteGeneratePreviewPixmap(pginfo);

        pixmap = gContext->LoadScalePixmap(filename);
        if (pixmap)
            return *pixmap;

        image = gContext->CacheRemotePixmap(filename);
    }

    if (image)
    {
        pixmap = new QPixmap();
 
        if (screenwidth != 800 || screenheight != 600)
        {
            QImage tmp2 = image->smoothScale((int)(image->width() * wmult),
                                             (int)(image->height() * hmult));
            pixmap->convertFromImage(tmp2);
        }
        else
        {
            pixmap->convertFromImage(*image);
        }
    }

    if (!pixmap)
    {
        QPixmap tmp((int)(160 * wmult), (int)(120 * hmult));
        tmp.fill(black);
        return tmp;
    }
 
    return *pixmap;
}

void ProgramListItem::paintCell(QPainter *p, const QColorGroup &cg,
                                int column, int width, int alignment)
{
    QColorGroup _cg(cg);
    QColor c = _cg.text();

    if (!pginfo->recording)
    {
        _cg.setColor(QColorGroup::Text, Qt::gray);
        _cg.setColor(QColorGroup::HighlightedText, Qt::gray);
    }
    else if (pginfo->conflicting)
    {
        _cg.setColor(QColorGroup::Text, Qt::red);
        _cg.setColor(QColorGroup::HighlightedText, Qt::red);
    }

    QListViewItem::paintCell(p, _cg, column, width, alignment);

    _cg.setColor(QColorGroup::Text, c);
}
