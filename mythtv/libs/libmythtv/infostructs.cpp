#include <qdatetime.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qstring.h>
#include <qsqlquery.h>
#include <qapplication.h>

#include "infostructs.h"
#include "mythcontext.h"

void ChannelInfo::LoadIcon(MythContext *context)
{
    QImage *tempimage = new QImage();

    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    icon = new QPixmap();

    if (tempimage->load(iconpath))
    {
        if (screenwidth != 800 || screenheight != 600 || 
            tempimage->width() != 40 || tempimage->height() != 40)
        {
            QImage tmp2;
            tmp2 = tempimage->smoothScale((int)(40 * wmult), 
                                          (int)(40 * hmult));
            icon->convertFromImage(tmp2);
        }
        else
            icon->convertFromImage(*tempimage);
    }

    delete tempimage;
}
