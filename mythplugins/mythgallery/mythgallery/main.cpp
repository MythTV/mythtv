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
    if (!gContext->TestPopupVersion("mythgallery", libversion, 
                                    MYTH_BINARY_VERSION))
        return -1;


    GallerySettings settings;
    settings.load(QSqlDatabase::database());
    settings.save(QSqlDatabase::database());    

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

