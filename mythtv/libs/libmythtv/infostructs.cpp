#include <qdatetime.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qstring.h>
#include <qsqlquery.h>
#include <qapplication.h>

#include "infostructs.h"
#include "settings.h"

extern Settings *globalsettings;

void ChannelInfo::LoadIcon(void)
{
    QImage *tempimage = new QImage();

    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    icon = new QPixmap();

    if (tempimage->load(iconpath))
    {
        if (screenwidth != 800 || screenheight != 600)
        {
            QImage tmp2 = tempimage->smoothScale(tempimage->width() * wmult, 
                                                 tempimage->height() * hmult);
            icon->convertFromImage(tmp2);
        }
        else
            icon->convertFromImage(*tempimage);
    }

    delete tempimage;
}
