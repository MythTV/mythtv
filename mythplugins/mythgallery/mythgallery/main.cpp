#include <iostream>

using namespace std;

#include <qapplication.h>

#include "iconview.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext();

    context->LoadSettingsFiles("mythgallery-settings.txt");

    context->LoadQtConfig();

    QString startdir = context->GetSetting("GalleryDir");

    IconView icv(context, startdir);

    icv.exec();

    return 0;
}

