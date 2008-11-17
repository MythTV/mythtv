#include <qdatetime.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qstring.h>
#include <qfile.h>
#include <qsqlquery.h>
#include <qapplication.h>

#include "infostructs.h"
#include "mythcontext.h"

void ChannelInfo::LoadChannelIcon(int width, int height)
{
    QImage tempimage(iconpath);

    if (!iconpath.isEmpty() && (tempimage.width() == 0))
    {
        QFile existtest(iconpath);

        // we have the file, just couldn't load it.
        if (existtest.exists())
            return;

        QString url = gContext->GetMasterHostPrefix();
        if (url.length() < 1)
            return;

        url += iconpath;

        MythImage *im = gContext->CacheRemotePixmap(url);
        if (im)
            tempimage = *(QImage*)im;
    }

    if (tempimage.width() > 0)
    {
        iconload = true;
        QImage tmp2;

        if ((height == 0) && (tempimage.width() != width
                                || tempimage.height() != width))
        {
            tmp2 = tempimage.scaled(width, width,
                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            icon = QPixmap::fromImage(tmp2);
        }
        else if ((height > 0) && (tempimage.width() != width
                                || tempimage.height() != height))
        {
            tmp2 = tempimage.scaled(width, height,
                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            icon = QPixmap::fromImage(tmp2);
        }
        else
            icon = QPixmap::fromImage(tempimage);
    }
}

QString ChannelInfo::Text(QString format)
{
    format.replace("<num>", chanstr);
    format.replace("<sign>", callsign);
    format.replace("<name>", channame);

    return format;
}

