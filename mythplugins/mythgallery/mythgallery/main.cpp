#include <iostream>

using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>

#include "iconview.h"
#include "gallerysettings.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

int mythplugin_init(const char *libversion)
{
    QString lib = libversion;
    if (lib != MYTH_BINARY_VERSION)
    {
        cerr << "This plugin was compiled against libmyth version: " 
             << MYTH_BINARY_VERSION 
             << "\nbut the library is version: " << libversion << endl;
        cerr << "You probably want to recompile everything, and do a\n"
             << "'make distclean' first.\n";
        return -1; 
    }

    return 0;
}

int mythplugin_run(void)
{
    QString startdir = gContext->GetSetting("GalleryDir");
    IconView icv(QSqlDatabase::database(), startdir, 
                 gContext->GetMainWindow(), "icon view");
    icv.exec();

    return 0;
}

int mythplugin_config(void)
{
    GallerySettings settings;
    settings.exec(QSqlDatabase::database());

    return 0;
}

