#include <qfile.h>
#include "pcrominfo.h"
#include <iostream>

using namespace std;

bool PCRomInfo::FindImage(QString type, QString *result)
{
    if (type != "screenshot")
        return false;

    QStringList graphic_formats;
    graphic_formats.append("png");
    graphic_formats.append("gif");
    graphic_formats.append("jpg");
    graphic_formats.append("jpeg");
    graphic_formats.append("xpm");
    graphic_formats.append("bmp");
    graphic_formats.append("pnm");
    graphic_formats.append("tif");
    graphic_formats.append("tiff");

    QString BaseFileName = gContext->GetSetting("PCScreensLocation") + "/" + gamename + ".";
    for (QStringList::Iterator i = graphic_formats.begin(); i != graphic_formats.end(); i++)
    {
        *result = BaseFileName + *i;
        if(QFile::exists(*result))
            return true;
    }
    return false;   
}

