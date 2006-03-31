#include <qdatetime.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qstring.h>
#include <qfile.h>
#include <qsqlquery.h>
#include <qapplication.h>

#include "infostructs.h"
#include "mythcontext.h"

void ChannelInfo::LoadIcon(int size)
{
    QImage tempimage(iconpath);

    if (tempimage.width() == 0)
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
        if (tempimage.width() != size || tempimage.height() != size)
        {
            QImage tmp2;
            tmp2 = tempimage.smoothScale(size, size);
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

