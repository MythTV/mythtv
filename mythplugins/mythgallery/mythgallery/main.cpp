#include <iostream>

using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>

#include "iconview.h"
#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythplugin.h>

extern "C" {
int mythplugin_init(void);
int mythplugin_run(void);
int mythplugin_config(void);
}

int mythplugin_init(void)
{
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
    return 0;
}

