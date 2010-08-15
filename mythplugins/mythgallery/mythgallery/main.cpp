// c++
#include <iostream>

// qt
#include <QDir>
#include <QtPlugin>

// myth
#include <mythcontext.h>
#include <mythversion.h>
#include <mythdialogs.h>
#include <mythmediamonitor.h>
#include <mythpluginapi.h>

// mythgallery
#include "config.h"
#include "iconview.h"
#include "gallerysettings.h"
#include "dbcheck.h"

#ifdef DCRAW_SUPPORT
Q_IMPORT_PLUGIN(dcrawplugin)
#endif // DCRAW_SUPPORT

static int run(MythMediaDevice *dev = NULL)
{
    QDir startdir(gCoreContext->GetSetting("GalleryDir"));
    if (startdir.exists() && startdir.isReadable())
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        IconView *iconview = new IconView(mainStack, "mythgallery",
                                          startdir.absolutePath(), dev);

        if (iconview->Create())
        {
            mainStack->AddScreen(iconview);
            return 0;
        }
        else
            delete iconview;
    }
    else
    {
        ShowOkPopup(QObject::tr("MythGallery cannot find its start directory."
                                "\n%1\n"
                                "Check the directory exists, is readable and "
                                "the setting is correct on MythGallery's "
                                "settings page.")
                    .arg(startdir.absolutePath()));
    }

    return -1;
}

void runGallery(void)
{
    run();
}

void handleMedia(MythMediaDevice *dev)
{
    if (dev && dev->isUsable())
        run(dev);
}

void setupKeys(void)
{
    REG_JUMP("MythGallery", QT_TRANSLATE_NOOP("MythControls",
        "Image viewer / slideshow"), "", runGallery);

    REG_KEY("Gallery", "PLAY", QT_TRANSLATE_NOOP("MythControls",
        "Start/Stop Slideshow"), "P");
    REG_KEY("Gallery", "HOME", QT_TRANSLATE_NOOP("MythControls",
        "Go to the first image in thumbnail view"), "Home");
    REG_KEY("Gallery", "END", QT_TRANSLATE_NOOP("MythControls",
        "Go to the last image in thumbnail view"), "End");
    REG_KEY("Gallery", "SLIDESHOW", QT_TRANSLATE_NOOP("MythControls",
        "Start Slideshow in thumbnail view"), "S");
    REG_KEY("Gallery", "RANDOMSHOW", QT_TRANSLATE_NOOP("MythControls",
        "Start Random Slideshow in thumbnail view"), "R");

    REG_KEY("Gallery", "ROTRIGHT", QT_TRANSLATE_NOOP("MythControls",
        "Rotate image right 90 degrees"), "],3");
    REG_KEY("Gallery", "ROTLEFT", QT_TRANSLATE_NOOP("MythControls",
        "Rotate image left 90 degrees"), "[,1");
    REG_KEY("Gallery", "ZOOMOUT", QT_TRANSLATE_NOOP("MythControls",
        "Zoom image out"), "7");
    REG_KEY("Gallery", "ZOOMIN", QT_TRANSLATE_NOOP("MythControls",
        "Zoom image in"), "9");
    REG_KEY("Gallery", "SCROLLUP", QT_TRANSLATE_NOOP("MythControls",
        "Scroll image up"), "2");
    REG_KEY("Gallery", "SCROLLLEFT", QT_TRANSLATE_NOOP("MythControls",
        "Scroll image left"), "4");
    REG_KEY("Gallery", "SCROLLRIGHT", QT_TRANSLATE_NOOP("MythControls",
        "Scroll image right"), "6");
    REG_KEY("Gallery", "SCROLLDOWN", QT_TRANSLATE_NOOP("MythControls",
        "Scroll image down"), "8");
    REG_KEY("Gallery", "RECENTER", QT_TRANSLATE_NOOP("MythControls",
        "Recenter image"), "5");
    REG_KEY("Gallery", "FULLSIZE", QT_TRANSLATE_NOOP("MythControls",
        "Full-size (un-zoom) image"), "0");
    REG_KEY("Gallery", "UPLEFT", QT_TRANSLATE_NOOP("MythControls",
        "Go to the upper-left corner of the image"), "PgUp");
    REG_KEY("Gallery", "LOWRIGHT", QT_TRANSLATE_NOOP("MythControls",
        "Go to the lower-right corner of the image"), "PgDown");
    REG_KEY("Gallery", "MARK", QT_TRANSLATE_NOOP("MythControls",
        "Mark image"), "T");
    REG_KEY("Gallery", "FULLSCREEN", QT_TRANSLATE_NOOP("MythControls",
        "Toggle scale to fullscreen/scale to fit"), "W");
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythGallery Media Handler 1/2"), "", "", handleMedia,
        MEDIATYPE_DATA | MEDIATYPE_MIXED, QString::null);
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythGallery Media Handler 2/2"), "", "", handleMedia,
        MEDIATYPE_MGALLERY, "gif,jpg,png");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythgallery", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gCoreContext->ActivateSettingsCache(false);
    UpgradeGalleryDatabaseSchema();
    gCoreContext->ActivateSettingsCache(true);

    GallerySettings settings;
    settings.Load();
    settings.Save();

    setupKeys();

    return 0;
}

int mythplugin_run(void)
{
    return run();
}

int mythplugin_config(void)
{
    GallerySettings settings;
    settings.exec();

    return 0;
}

