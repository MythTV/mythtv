#include <mythtv/mythcontext.h>

#include "gallerysettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

#include "config.h"

// General Settings

static HostLineEdit *MythGalleryDir()
{
    HostLineEdit *gc = new HostLineEdit("GalleryDir");
    gc->setLabel(QObject::tr("Directory that holds images"));
    gc->setValue("/var/lib/pictures");
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

static HostLineEdit *MythGalleryMoviePlayerCmd()
{
    HostLineEdit *gc = new HostLineEdit("GalleryMoviePlayerCmd");
    gc->setLabel(QObject::tr("Command run to display movie files"));
    gc->setValue("mplayer -fs %s");
    gc->setHelpText(QObject::tr("This command is executed whenever a movie "
				            "file is selected"));
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

#ifdef OPENGL_SUPPORT

static HostCheckBox *SlideshowUseOpenGL()
{
    HostCheckBox *gc = new HostCheckBox("SlideshowUseOpenGL");
    gc->setLabel(QObject::tr("Use OpenGL transitions"));
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
    gc->addSelection("fade (gl)");
    gc->addSelection("rotate (gl)");
    gc->addSelection("bend (gl)");
    gc->addSelection("inout (gl)");
    gc->addSelection("slide (gl)");
    gc->addSelection("flutter (gl)");
    gc->addSelection("cube (gl)");
    gc->addSelection("random (gl)");
    gc->setHelpText(QObject::tr("This is the type of OpenGL transition used "
                    "between pictures in slideshow mode."));
    return gc;
};

#endif /* OPENGL_SUPPORT */

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
    HostSpinBox *gc = new HostSpinBox("SlideshowDelay", 1, 600, 1);
    gc->setLabel(QObject::tr("Slideshow Delay"));
    gc->setValue(5);
    gc->setHelpText(QObject::tr("This is the number of seconds to display each "
                    "picture."));
    return gc;
};


class GalleryConfigurationGroup: public VerticalConfigurationGroup,
                                 public TriggeredConfigurationGroup {
public:

    GalleryConfigurationGroup():
        VerticalConfigurationGroup(false),
        TriggeredConfigurationGroup(false) {
        setLabel(QObject::tr("MythGallery Settings"));
        setUseLabel(false);

        addChild(MythGalleryDir());
        addChild(MythGalleryThumbnailLocation());
        addChild(MythGalleryImportDirs());
        addChild(MythGalleryMoviePlayerCmd());

#ifdef OPENGL_SUPPORT
        
        HostCheckBox* useOpenGL = SlideshowUseOpenGL();
        addChild(useOpenGL);
        setTrigger(useOpenGL);
    
        ConfigurationGroup* openGLConfig = new VerticalConfigurationGroup(false);
        openGLConfig->addChild(SlideshowOpenGLTransition());
        addTarget("1", openGLConfig);

        ConfigurationGroup* regularConfig = new VerticalConfigurationGroup(false);
        regularConfig->addChild(SlideshowTransition());
        regularConfig->addChild(SlideshowBackground());
        addTarget("0", regularConfig);

#else
        
        addChild(SlideshowTransition());
        addChild(SlideshowBackground());
        
#endif

        
        
        addChild(SlideshowDelay());

    }

};


GallerySettings::GallerySettings()
{
    GalleryConfigurationGroup* config = new GalleryConfigurationGroup();
    addChild(config);
}

