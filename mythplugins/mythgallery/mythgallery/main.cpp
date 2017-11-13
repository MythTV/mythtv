// c++
#include <iostream>

// qt
#include <QDir>
#include <QtPlugin>
#include <QImageReader>
#include <QCoreApplication>

// myth
#include <mythcontext.h>
#include <mythversion.h>
#include <mythmediamonitor.h>
#include <mythpluginapi.h>

// mythgallery
#include "config.h"
#include "iconview.h"
#include "gallerysettings.h"
#include "galleryutil.h"
#include "dbcheck.h"

#ifdef DCRAW_SUPPORT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // Qt5 imports class name
    Q_IMPORT_PLUGIN(DcrawPlugin)
#else
    Q_IMPORT_PLUGIN(dcrawplugin)
#endif // QT_VERSION
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
        ShowOkPopup(QCoreApplication::translate("(MythGalleryMain)",
            "MythGallery cannot find its start directory.\n"
            "%1\n"
            "Check the directory exists, is readable and the setting is "
            "correct on MythGallery's settings page.")
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
        run(dev);
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
        "Start slideshow in thumbnail view"), "S");
    REG_KEY("Gallery", "RANDOMSHOW", QT_TRANSLATE_NOOP("MythControls",
        "Start random slideshow in thumbnail view"), "R");
#ifdef EXIF_SUPPORT
    REG_KEY("Gallery", "SEASONALSHOW", QT_TRANSLATE_NOOP("MythControls",
        "Start random slideshow with seasonal theme in thumbnail view"), "");
#endif // EXIF_SUPPORT

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
        "MythGallery Media Handler 1/3"), "", handleMedia,
        MEDIATYPE_DATA | MEDIATYPE_MIXED, QString::null);
    QString filt;
    Q_FOREACH(QString format, GalleryUtil::GetImageFilter())
    {
        format.remove(0,2); // Remoce "*."
        if (filt.isEmpty())
            filt = format;
        else
            filt += "," + format;
    }
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythGallery Media Handler 2/3"), "", handleMedia,
        MEDIATYPE_MGALLERY, filt);
    filt.clear();
    Q_FOREACH(QString format, GalleryUtil::GetMovieFilter())
    {
        format.remove(0,2); // Remoce "*."
        if (filt.isEmpty())
            filt = format;
        else
            filt += "," + format;
    }
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythGallery Media Handler 3/3"), "", handleMedia,
        MEDIATYPE_MVIDEO, filt);
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
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    StandardSettingDialog *ssd =
        new StandardSettingDialog(mainStack, "gallerysettings",
                                  new GallerySettings());

    if (ssd->Create())
    {
        mainStack->AddScreen(ssd);
    }
    else
        delete ssd;

    return 0;
}

