
// qt
#include <QDir>

// myth
#include <mythcontext.h>

// mythgallery
#include "config.h"
#include "gallerysettings.h"
#include "galleryfilter.h"

// General Settings

static HostLineEdit *MythGalleryFilter()
{
    HostLineEdit *gc = new HostLineEdit("GalleryFilterDirectory");
    gc->setLabel(GallerySettings::tr("Directory filter"));
    gc->setValue("");
    gc->setHelpText(GallerySettings::tr("Enter directory names to be excluded "
                                        "in browser. (multiple entries "
                                        "delimited with ':')"));
    return gc;
};

static HostComboBox *MythGalleryFilterType()
{
    HostComboBox *gc = new HostComboBox("GalleryFilterType");
    gc->setLabel(GallerySettings::tr("Type filter"));
    gc->addSelection(GallerySettings::tr("All"),
                     QString::number(kTypeFilterAll));
    gc->addSelection(GallerySettings::tr("Images only"),
                     QString::number(kTypeFilterImagesOnly));
    gc->addSelection(GallerySettings::tr("Movies only"),
                     QString::number(kTypeFilterMoviesOnly));
    gc->setHelpText(GallerySettings::tr("This is the type filter for the "
                                        "displayed thumbnails."));
    return gc;
};

static HostLineEdit *MythGalleryDir()
{
    HostLineEdit *gc = new HostLineEdit("GalleryDir");
    gc->setLabel(GallerySettings::tr("Directory that holds images"));
#ifdef Q_OS_MAC
    gc->setValue(QDir::homePath() + "/Pictures");
#else
    gc->setValue("/var/lib/pictures");
#endif
    gc->setHelpText(GallerySettings::tr("This directory must exist and "
                                        "MythGallery needs to have read "
                                        "permission."));
    return gc;
};

static HostCheckBox *MythGalleryThumbnailLocation()
{
    HostCheckBox *gc = new HostCheckBox("GalleryThumbnailLocation");
    gc->setLabel(GallerySettings::tr("Store thumbnails in image directory"));
    gc->setValue(true);
    gc->setHelpText(GallerySettings::tr("If set, thumbnails are stored in "
                                        "'.thumbcache' directories within "
                                        "the above directory. If cleared, "
                                        "they are stored in your home "
                                        "directory."));
    return gc;
};

static HostComboBox *MythGallerySortOrder()
{
    HostComboBox *gc = new HostComboBox("GallerySortOrder");
    gc->setLabel(GallerySettings::tr("Sort order when browsing"));
    gc->addSelection(GallerySettings::tr("Unsorted"),
                     QString::number(kSortOrderUnsorted));
    gc->addSelection(GallerySettings::tr("Name (A-Z alpha)"),
                     QString::number(kSortOrderNameAsc));
    gc->addSelection(GallerySettings::tr("Reverse Name (Z-A alpha)"),
                     QString::number(kSortOrderNameDesc));
    gc->addSelection(GallerySettings::tr("Mod Time (oldest first)"),
                     QString::number(kSortOrderModTimeAsc));
    gc->addSelection(GallerySettings::tr("Reverse Mod Time (newest first)"),
                     QString::number(kSortOrderModTimeDesc));
    gc->addSelection(GallerySettings::tr("Extension (A-Z alpha)"),
                     QString::number(kSortOrderExtAsc));
    gc->addSelection(GallerySettings::tr("Reverse Extension (Z-A alpha)"),
                     QString::number(kSortOrderExtDesc));
    gc->addSelection(GallerySettings::tr("Filesize (smallest first)"),
                     QString::number(kSortOrderSizeAsc));
    gc->addSelection(GallerySettings::tr("Reverse Filesize (largest first)"),
                     QString::number(kSortOrderSizeDesc));
    gc->setHelpText(GallerySettings::tr("This is the sort order for the "
                    "displayed picture thumbnails."));
    return gc;
};

static HostSpinBox *MythGalleryOverlayCaption()
{
    HostSpinBox *gc = new HostSpinBox("GalleryOverlayCaption", 0, 600, 1);
    gc->setLabel(GallerySettings::tr("Overlay caption"));
    gc->setValue(0);
    gc->setHelpText(GallerySettings::tr("This is the number of seconds to show "
                    "a caption on top of a full size picture."));
    return gc;
};

static HostLineEdit *MythGalleryImportDirs()
{
    HostLineEdit *gc = new HostLineEdit("GalleryImportDirs");
    gc->setLabel(GallerySettings::tr("Paths to import images from"));
    gc->setValue("/mnt/cdrom:/mnt/camera");
    gc->setHelpText(GallerySettings::tr("This is a colon separated list of "
                    "paths. If the path in the list is a directory, its "
                    "contents will be copied. If it is an executable, "
                    "it will be run."));
    return gc;
};

static HostCheckBox *MythGalleryAutoLoad()
{
    HostCheckBox *gc = new HostCheckBox("GalleryAutoLoad");
    gc->setLabel(GallerySettings::tr("Automatically load MythGallery to "
                                     "display pictures"));
    gc->setValue(false);
    gc->setHelpText(GallerySettings::tr("When a new CD-Rom or removable "
                                        "storage device containing pictures is "
                                        "detected then load MythGallery to "
                                        "display the content."));
    return gc;
}

#ifdef USING_OPENGL

static HostCheckBox *SlideshowUseOpenGL()
{
    HostCheckBox *gc = new HostCheckBox("SlideshowUseOpenGL");
    gc->setLabel(GallerySettings::tr("Use OpenGL transitions"));
    gc->setValue(false);
    gc->setHelpText(GallerySettings::tr("Check this to enable OpenGL "
                                        "based slideshow transitions"));
    return gc;
};

static HostComboBox *SlideshowOpenGLTransition()
{
    HostComboBox *gc = new HostComboBox("SlideshowOpenGLTransition");
    gc->setLabel(GallerySettings::tr("Type of OpenGL transition"));

    //: No OpenGL transition
    gc->addSelection(GallerySettings::tr("none"), "none");
    //: Blend OpenGL transition
    gc->addSelection(GallerySettings::tr("blend (gl)"), "blend (gl)");
    //: Room blend OpenGL transition
    gc->addSelection(GallerySettings::tr("zoom blend (gl)"), "zoom blend (gl)");
    //: Fade OpenGL transition
    gc->addSelection(GallerySettings::tr("fade (gl)"), "fade (gl)");
    //: Rotate OpenGL transition
    gc->addSelection(GallerySettings::tr("rotate (gl)"), "rotate (gl)");
    //: Bend OpenGL transition
    gc->addSelection(GallerySettings::tr("bend (gl)"), "bend (gl)");
    //: Inout OpenGL transition
    gc->addSelection(GallerySettings::tr("inout (gl)"), "inout (gl)");
    //: Slide OpenGL transition
    gc->addSelection(GallerySettings::tr("slide (gl)"), "slide (gl)");
    //: Flutter OpenGL transition
    gc->addSelection(GallerySettings::tr("flutter (gl)"), "flutter (gl)");
    //: Cube OpenGL transition
    gc->addSelection(GallerySettings::tr("cube (gl)"), "cube (gl)");
    //: Ken Burns OpenGL transition
    gc->addSelection(GallerySettings::tr("Ken Burns (gl)"), "Ken Burns (gl)");
    //: Random OpenGL transition
    gc->addSelection(GallerySettings::tr("random (gl)"), "random (gl)");

    gc->setHelpText(GallerySettings::tr("This is the type of OpenGL transition "
                                        "used between pictures in slideshow "
                                        "mode."));
    return gc;
};

static HostSpinBox *SlideshowOpenGLTransitionLength()
{
    HostSpinBox *gc = new HostSpinBox(
        "SlideshowOpenGLTransitionLength", 500, 120000, 500);
    gc->setLabel(GallerySettings::tr("Duration of OpenGL Transition "
                                     "(milliseconds)"));
    gc->setValue(2000);
    return gc;
};

#endif /* USING_OPENGL */

static HostComboBox *SlideshowTransition()
{
    HostComboBox *gc = new HostComboBox("SlideshowTransition");
    gc->setLabel(GallerySettings::tr("Type of transition"));

    gc->addSelection(GallerySettings::tr("none",
                                         "Slideshow transition"),
                                         "none");
    gc->addSelection(GallerySettings::tr("chess board",
                                         "Slideshow transition"),
                                         "chess board");
    gc->addSelection(GallerySettings::tr("melt down",
                                         "Slideshow transition"),
                                         "melt down");
    gc->addSelection(GallerySettings::tr("sweep",
                                         "Slideshow transition"),
                                         "sweep");
    gc->addSelection(GallerySettings::tr("noise",
                                         "Slideshow transition"), 
                                         "noise");
    gc->addSelection(GallerySettings::tr("growing",
                                         "Slideshow transition"),
                                         "growing");
    gc->addSelection(GallerySettings::tr("incoming edges",
                                         "Slideshow transition"),
                                         "incoming edges");
    gc->addSelection(GallerySettings::tr("horizontal lines", 
                                         "Slideshow transition"),
                                         "horizontal lines");
    gc->addSelection(GallerySettings::tr("vertical lines",
                                         "Slideshow transition"),
                                         "vertical lines");
    gc->addSelection(GallerySettings::tr("circle out", "Slideshow transition"),
                                         "circle out");
    gc->addSelection(GallerySettings::tr("multicircle out", 
                                         "Slideshow transition"), 
                                         "multicircle out");
    gc->addSelection(GallerySettings::tr("spiral in",
                                         "Slideshow transition"),
                                         "spiral in");
    gc->addSelection(GallerySettings::tr("blobs",
                                         "Slideshow transition"),
                                         "blobs");
    gc->addSelection(GallerySettings::tr("random",
                                         "Slideshow transition"),
                                         "random");

    gc->setHelpText(GallerySettings::tr("This is the type of transition used "
                                        "between pictures in slideshow mode."));
    return gc;
};

static HostComboBox *SlideshowBackground()
{
    HostComboBox *gc = new HostComboBox("SlideshowBackground");
    gc->setLabel(GallerySettings::tr("Type of background"));
    // use names from /etc/X11/rgb.txt
    gc->addSelection(GallerySettings::tr("theme",
                                         "Slideshow background"),
                                         "");
    gc->addSelection(GallerySettings::tr("black",
                                         "Slideshow background"),
                                         "black");
    gc->addSelection(GallerySettings::tr("white",
                                         "Slideshow background"),
                                         "white");
    gc->setHelpText(GallerySettings::tr("This is the type of background for "
                                        "each picture in single view mode."));
    return gc;
};

static HostSpinBox *SlideshowDelay()
{
    HostSpinBox *gc = new HostSpinBox("SlideshowDelay", 0, 86400, 1);
    gc->setLabel(GallerySettings::tr("Slideshow Delay"));
    gc->setValue(5);
    gc->setHelpText(GallerySettings::tr("This is the number of seconds to "
                                        "display each picture."));
    return gc;
};

static HostCheckBox *SlideshowRecursive()
{
    HostCheckBox *gc = new HostCheckBox("GalleryRecursiveSlideshow");
    gc->setLabel(GallerySettings::tr("Recurse into directories"));
    gc->setHelpText(GallerySettings::tr("Check this to let the slideshow "
                                        "recurse into sub-directories."));
    return gc;
};

class GalleryConfigurationGroup : public TriggeredConfigurationGroup
{
  public:
    GalleryConfigurationGroup() :
        TriggeredConfigurationGroup(false, true, false, false)
    {
        setLabel(GallerySettings::tr("MythGallery Settings (Slideshow)"));
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
    general->setLabel(tr("MythGallery Settings (General)"));
    general->addChild(MythGalleryDir());
    general->addChild(MythGalleryThumbnailLocation());
    general->addChild(MythGallerySortOrder());
    general->addChild(MythGalleryImportDirs());
    general->addChild(MythGalleryAutoLoad());
    general->addChild(MythGalleryFilter());
    general->addChild(MythGalleryFilterType());
    addChild(general);

    GalleryConfigurationGroup* config = new GalleryConfigurationGroup();
    addChild(config);
}

