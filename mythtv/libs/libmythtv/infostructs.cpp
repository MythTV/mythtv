#include <qdatetime.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qstring.h>
#include <qsqlquery.h>
#include <qapplication.h>

#include "infostructs.h"

void ChannelInfo::LoadIcon(void)
{
    QImage *tempimage = new QImage();

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    screenheight = 600; screenwidth = 800;
     
    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    icon = new QPixmap(30 * wmult, 30 * hmult);

    if (tempimage->load(iconpath))
    {
        if (screenwidth != 800 || screenheight != 600)
        {
            QImage tmp2 = tempimage->smoothScale(30 * wmult, 30 * hmult);
            icon->convertFromImage(tmp2);
        }
        else
            icon->convertFromImage(*tempimage);
    }

    delete tempimage;
}
