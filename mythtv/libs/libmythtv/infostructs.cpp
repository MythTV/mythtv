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

        QImage *cached = gContext->CacheRemotePixmap(url);
        if (cached)
            tempimage = *cached;
    }

    if (tempimage.width() > 0)
    {
        iconload = true;
        QImage tmp2;

        if ((height == 0) && (tempimage.width() != width
                                || tempimage.height() != width))
        {
            tmp2 = tempimage.smoothScale(width, width);
            icon.convertFromImage(tmp2);
        }
        else if ((height > 0) && (tempimage.width() != width
                                || tempimage.height() != height))
        {
            tmp2 = tempimage.smoothScale(width, height);
            icon.convertFromImage(tmp2);
        }
        else
            icon.convertFromImage(tempimage);
    }
}

QString ChannelInfo::Text(QString format)
{
    format.replace("<num>", chanstr);
    format.replace("<sign>", callsign);
    format.replace("<name>", channame);

    return format;
}

