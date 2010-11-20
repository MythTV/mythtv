
// qt
#include <QDir>

// myth
#include <mythcontext.h>

// mythgallery
#include "config.h"
#include "gallerysettings.h"

// General Settings

static HostLineEdit *MythGalleryDir()
{
    HostLineEdit *gc = new HostLineEdit("GalleryDir");
    gc->setLabel(QObject::tr("Directory that holds images"));
#ifdef Q_WS_MACX
    gc->setValue(QDir::homePath() + "/Pictures");
#else
    gc->setValue("/var/lib/pictures");
#endif
    gc->setHelpText(QObject::tr("This directory must exist and "
                       "MythGallery needs to have read permission."));
    return gc;
};

static HostCheckBox *MythGalleryThumbnailLocation()
{
    HostCheckBox *gc = new HostCheckBox("GalleryThumbnailLocation");
    gc->setLabel(QObject::tr("Store thumbnails in image directory"));
    gc->setValue(true);
    gc->setHelpText(QObject::tr("If set, thumbnails are stored in '.thumbcache' directories within the above directory. If cleared, they are stored in your home directory."));
    return gc;
};

static HostComboBox *MythGallerySortOrder()
{
    HostComboBox *gc = new HostComboBox("GallerySortOrder");
    gc->setLabel(QObject::tr("Sort order when browsing"));
    gc->addSelection("Name (A-Z alpha)", QString::number(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase));
    gc->addSelection("Reverse Name (Z-A alpha)", QString::number(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase | QDir::Reversed));
    gc->addSelection("Mod Time (earliest first)", QString::number(QDir::Time | QDir::DirsFirst | QDir::IgnoreCase | QDir::Reversed));
    gc->addSelection("Reverse Mod Time (most recent first)", QString::number(QDir::Time | QDir::DirsFirst | QDir::IgnoreCase));
    gc->setHelpText(QObject::tr("This is the sort order for the displayed "
                    "picture thumbnails."));
    return gc;
};

static HostLineEdit *MythGalleryMoviePlayerCmd()
{
    HostLineEdit *gc = new HostLineEdit("GalleryMoviePlayerCmd");
    gc->setLabel(QObject::tr("Command run to display movie files"));
    gc->setValue("mplayer -fs %s");
    gc->setHelpText(QObject::tr("This command is executed whenever a movie "
                    "file is selected"));
    return gc;
};

static HostSpinBox *MythGalleryOverlayCaption()
{
    HostSpinBox *gc = new HostSpinBox("GalleryOverlayCaption", 0, 600, 1);
    gc->setLabel(QObject::tr("Overlay caption"));
    gc->setValue(0);
    gc->setHelpText(QObject::tr("This is the number of seconds to show a caption "
                    "on top of a full size picture."));
    return gc;
};

static HostLineEdit *MythGalleryImportDirs()
{
    HostLineEdit *gc = new HostLineEdit("GalleryImportDirs");
    gc->setLabel(QObject::tr("Paths to import images from"));
    gc->setValue("/mnt/cdrom:/mnt/camera");
    gc->setHelpText(QObject::tr("This is a colon separated list of paths. "
                    "If the path in the list is a directory, its contents will "
                    "be copied. If it is an executable, it will be run."));
    return gc;
};

#ifdef USING_OPENGL

static HostCheckBox *SlideshowUseOpenGL()
{
    HostCheckBox *gc = new HostCheckBox("SlideshowUseOpenGL");
    gc->setLabel(QObject::tr("Use OpenGL transitions"));
    gc->setValue(false);
    gc->setHelpText(QObject::tr("Check this to enable OpenGL "
                                "based slideshow transitions"));
    return gc;
};

static HostComboBox *SlideshowOpenGLTransition()
{
    HostComboBox *gc = new HostComboBox("SlideshowOpenGLTransition");
    gc->setLabel(QObject::tr("Type of OpenGL transition"));
    gc->addSelection("none");
    gc->addSelection("blend (gl)");
    gc->addSelection("zoom blend (gl)");
    gc->addSelection("fade (gl)");
    gc->addSelection("rotate (gl)");
    gc->addSelection("bend (gl)");
    gc->addSelection("inout (gl)");
    gc->addSelection("slide (gl)");
    gc->addSelection("flutter (gl)");
    gc->addSelection("cube (gl)");
    gc->addSelection("Ken Burns (gl)");
    gc->addSelection("random (gl)");
    gc->setHelpText(QObject::tr("This is the type of OpenGL transition used "
                    "between pictures in slideshow mode."));
    return gc;
};

static HostSpinBox *SlideshowOpenGLTransitionLength()
{
    HostSpinBox *gc = new HostSpinBox(
        "SlideshowOpenGLTransitionLength", 500, 30000, 500);
    gc->setLabel(QObject::tr("Duration of OpenGL Transition (milliseconds)"));
    gc->setValue(2000);
    return gc;
};

#endif /* USING_OPENGL */

static HostComboBox *SlideshowTransition()
{
    HostComboBox *gc = new HostComboBox("SlideshowTransition");
    gc->setLabel(QObject::tr("Type of transition"));
    gc->addSelection("none");
    gc->addSelection("chess board");
    gc->addSelection("melt down");
    gc->addSelection("sweep");
    gc->addSelection("noise");
    gc->addSelection("growing");
    gc->addSelection("incoming edges");
    gc->addSelection("horizontal lines");
    gc->addSelection("vertical lines");
    gc->addSelection("circle out");
    gc->addSelection("multicircle out");
    gc->addSelection("spiral in");
    gc->addSelection("blobs");
    gc->addSelection("random");
    gc->setHelpText(QObject::tr("This is the type of transition used "
                    "between pictures in slideshow mode."));
    return gc;
};

static HostComboBox *SlideshowBackground()
{
    HostComboBox *gc = new HostComboBox("SlideshowBackground");
    gc->setLabel(QObject::tr("Type of background"));
    // use names from /etc/X11/rgb.txt
    gc->addSelection("theme","");
    gc->addSelection("black");
    gc->addSelection("white");
    gc->setHelpText(QObject::tr("This is the type of background for each "
                    "picture in single view mode."));
    return gc;
};

static HostSpinBox *SlideshowDelay()
{
    HostSpinBox *gc = new HostSpinBox("SlideshowDelay", 0, 600, 1);
    gc->setLabel(QObject::tr("Slideshow Delay"));
    gc->setValue(5);
    gc->setHelpText(QObject::tr("This is the number of seconds to display each "
                    "picture."));
    return gc;
};

static HostCheckBox *SlideshowRecursive()
{
    HostCheckBox *gc = new HostCheckBox("GalleryRecursiveSlideshow");
    gc->setLabel(QObject::tr("Recurse into directories"));
    gc->setHelpText(QObject::tr("Check this to let the slideshow recurse into "
                                "sub-directories."));
    return gc;
};

class GalleryConfigurationGroup : public TriggeredConfigurationGroup
{
  public:
    GalleryConfigurationGroup() :
        TriggeredConfigurationGroup(false, true, false, false)
    {
        setLabel(QObject::tr("MythGallery Settings (Slideshow)"));
        setUseLabel(false);

#ifdef USING_OPENGL
        HostCheckBox* useOpenGL = SlideshowUseOpenGL();
        addChild(useOpenGL);
        setTrigger(useOpenGL);

        ConfigurationGroup* openGLConfig = new VerticalConfigurationGroup(false);
        openGLConfig->addChild(SlideshowOpenGLTransition());
        openGLConfig->addChild(SlideshowOpenGLTransitionLength());
        addTarget("1", openGLConfig);
#endif

        ConfigurationGroup* regularConfig = new VerticalConfigurationGroup(false);
        regularConfig->addChild(MythGalleryOverlayCaption());
        regularConfig->addChild(SlideshowTransition());
        regularConfig->addChild(SlideshowBackground());
        addTarget("0", regularConfig);

        addChild(SlideshowDelay());
        addChild(SlideshowRecursive());
    }

};


GallerySettings::GallerySettings()
{
    VerticalConfigurationGroup* general = new VerticalConfigurationGroup(false);
    general->setLabel(QObject::tr("MythGallery Settings (General)"));
    general->addChild(MythGalleryDir());
    general->addChild(MythGalleryThumbnailLocation());
    general->addChild(MythGallerySortOrder());
    general->addChild(MythGalleryImportDirs());
    general->addChild(MythGalleryMoviePlayerCmd());
    addChild(general);

    GalleryConfigurationGroup* config = new GalleryConfigurationGroup();
    addChild(config);
}

