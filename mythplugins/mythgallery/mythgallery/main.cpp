#include <iostream>

using namespace std;

#include <qapplication.h>

#include "iconview.h"

#include <mythtv/themedmenu.h>
#include <mythtv/settings.h>

Settings *globalsettings;
char theprefix[] = "/usr/local";

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    globalsettings = new Settings();

    globalsettings->LoadSettingsFiles("mythgallery-settings.txt", theprefix);
    globalsettings->LoadSettingsFiles("theme.txt", theprefix);

    QString themename = globalsettings->GetSetting("Theme");
    QString themedir = findThemeDir(themename, theprefix);

    globalsettings->SetSetting("ThemePathName", themedir + "/");

    themedir += "/qtlook.txt";
    globalsettings->ReadSettings(themedir);

    QString startdir = globalsettings->GetSetting("GalleryDir");

    IconView icv(startdir);

    icv.exec();

    return 0;
}

