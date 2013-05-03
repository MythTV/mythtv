// c++
#include <iostream>

// qt
#include <QDir>
#include <QtPlugin>
#include <QImageReader>

// myth
#include <mythcontext.h>
#include <mythversion.h>
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
void runRandomSlideshow(void);

static int run(MythMediaDevice *dev = NULL, bool startRandomShow = false)
{
    QDir startdir(gCoreContext->GetSetting("GalleryDir"));
    if (startdir.exists() && startdir.isReadable())
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        IconView *iconview = new IconView(mainStack, "mythgallery",
                                          startdir.absolutePath(), dev);

        if (iconview->Create())
        {
            if (startRandomShow)
            {
                iconview->HandleRandomShow();
            }
            else
            {
                mainStack->AddScreen(iconview);
            }
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

static void runGallery(void)
{
    run();
}

void runRandomSlideshow(void)
{
    run(NULL, true);
}

static void handleMedia(MythMediaDevice *dev)
{
    if (! gCoreContext->GetNumSetting("GalleryAutoLoad", 0))
        return;

    if (dev && dev->isUsable())
    {
        GetMythMainWindow()->JumpTo("Main Menu");
        run(dev);
    }
}

static void setupKeys(void)
{
    REG_JUMP("MythGallery", QT_TRANSLATE_NOOP("MythControls",
        "Image viewer / slideshow"), "", runGallery);
    REG_JUMP("Random Slideshow", QT_TRANSLATE_NOOP("MythControls",
        "Start Random Slideshow in thumbnail view"), "", runRandomSlideshow);

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
    QString filt;
    Q_FOREACH(QByteArray format, QImageReader::supportedImageFormats())
    {
        if (filt.isEmpty())
            filt = format;
        else
            filt += "," + format;
    }
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythGallery Media Handler 2/2"), "", "", handleMedia,
        MEDIATYPE_MGALLERY, filt);
}

int mythplugin_init(const char *libversion)
{
    if (!gCoreContext->TestPluginVersion("mythgallery", libversion,
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

