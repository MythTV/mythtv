#include <qimage.h>
#include <qpixmap.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "tv.h"
#include "infostructs.h"
#include "programlistitem.h"

ProgramListItem::ProgramListItem(QListView *parent, ProgramInfo *lpginfo,
                                 RecordingInfo *lrecinfo, int numcols,
                                 TV *ltv, QString lprefix)
               : QListViewItem(parent)
{
    prefix = lprefix;
    tv = ltv;
    pginfo = lpginfo;
    recinfo = lrecinfo;
    pixmap = NULL;
    setText(0, pginfo->channum);
    setText(1, pginfo->startts.toString("MMMM d h:mm AP"));
    setText(2, pginfo->title);

    if (numcols == 4)
    {
        string filename;

        recinfo->GetRecordFilename(prefix.ascii(), filename);

        struct stat64 st;
 
        long long size = 0;
        if (stat64(filename.c_str(), &st) == 0)
            size = st.st_size;
        long int mbytes = size / 1024 / 1024;
        QString filesize = QString("%1 MB").arg(mbytes);

        setText(3, filesize);
    }
}

QPixmap *ProgramListItem::getPixmap(void)
{
    if (pixmap)
        return pixmap;

    string filename;
    recinfo->GetRecordFilename(prefix.ascii(), filename);
    filename += ".png";

    pixmap = new QPixmap();

    if (pixmap->load(filename.c_str()))
        return pixmap;

    int len = 0;
    unsigned char *data = (unsigned char *)tv->GetScreenGrab(recinfo, 65, len);

    QImage img(data, 480, 480, 32, NULL, 65536 * 65536, QImage::LittleEndian);
    img = img.smoothScale(160, 120);

    img.save(filename.c_str(), "PNG");

    delete [] data;

    if (pixmap->load(filename.c_str()))
        return pixmap;

    return NULL;
}
