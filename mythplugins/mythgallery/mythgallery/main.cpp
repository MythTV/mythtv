#include <iostream>

using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>

#include "iconview.h"
#include "gallerysettings.h"
#include "dbcheck.h"

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


    UpgradeGalleryDatabaseSchema();

    GallerySettings settings;
    settings.load(QSqlDatabase::database());
    settings.save(QSqlDatabase::database());    

    return 0;
}

int mythplugin_run(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythgallery_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    QString startdir = gContext->GetSetting("GalleryDir");
    IconView icv(QSqlDatabase::database(), startdir, 
                 gContext->GetMainWindow(), "icon view");
    icv.exec();

    qApp->removeTranslator(&translator);
    return 0;
}

int mythplugin_config(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythgallery_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    GallerySettings settings;
    settings.exec(QSqlDatabase::database());

    qApp->removeTranslator(&translator);
    return 0;
}

