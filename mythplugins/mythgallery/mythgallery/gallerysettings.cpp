#include <mythtv/mythcontext.h>

#include "gallerysettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

#include "config.h"

// General Settings

class MythGalleryDir: public GlobalLineEdit {
public:
    MythGalleryDir():
        GlobalLineEdit("GalleryDir") {
        setLabel(QObject::tr("Directory that holds images"));
        setValue("/var/lib/pictures");
        setHelpText(QObject::tr("This directory must exist and "
                       "MythGallery needs to have read permission."));
    };
};

class MythGalleryMoviePlayerCmd: public GlobalLineEdit {
public:
    MythGalleryMoviePlayerCmd():
        GlobalLineEdit("GalleryMoviePlayerCmd") {
        setLabel(QObject::tr("Command run to display movie files"));
        setValue("mplayer -fs %s");
        setHelpText(QObject::tr("This command is executed whenever a movie "
				"file is selected"));
    };
};

class MythGalleryImportDirs: public GlobalLineEdit {
public:
    MythGalleryImportDirs():
        GlobalLineEdit("GalleryImportDirs") {
        setLabel(QObject::tr("Paths to import images from"));
        setValue("/mnt/cdrom:/mnt/camera");
        setHelpText(QObject::tr("This is a colon separated list of paths. "
                    "If the path in the list is a directory, its contents will "
                    "be copied. If it is an executable, it will be run."));
    };
};

#ifdef OPENGL_SUPPORT

class SlideshowUseOpenGL: public GlobalCheckBox {
public:

    SlideshowUseOpenGL() :
        GlobalCheckBox("SlideshowUseOpenGL") {
        setLabel(QObject::tr("Use OpenGL transitions"));
        setHelpText(QObject::tr("Check this to enable OpenGL "
                                "based slideshow transitions"));
    }
};

class SlideshowOpenGLTransition: public GlobalComboBox {
public:
    SlideshowOpenGLTransition() : 
        GlobalComboBox("SlideshowOpenGLTransition") {
        setLabel(QObject::tr("Type of OpenGL transition"));
        addSelection("none");
        addSelection("blend (gl)");
        addSelection("fade (gl)");
        addSelection("rotate (gl)");
        addSelection("bend (gl)");
        addSelection("inout (gl)");
        addSelection("slide (gl)");
        addSelection("flutter (gl)");
        addSelection("cube (gl)");
        addSelection("random (gl)");
        setHelpText(QObject::tr("This is the type of OpenGL transition used "
                    "between pictures in slideshow mode."));
    }
};

#endif /* OPENGL_SUPPORT */

class SlideshowTransition: public GlobalComboBox {
public:
    SlideshowTransition() : 
        GlobalComboBox("SlideshowTransition") {
        setLabel(QObject::tr("Type of transition"));
        addSelection("none");
        addSelection("chess board"); 
        addSelection("melt down");
        addSelection("sweep");
        addSelection("noise");
        addSelection("growing");
        addSelection("incoming edges");
        addSelection("horizontal lines");
        addSelection("vertical lines");
        addSelection("circle out");
        addSelection("multicircle out");
        addSelection("spiral in");
        addSelection("blobs");
        addSelection("random");
        setHelpText(QObject::tr("This is the type of transition used "
                    "between pictures in slideshow mode."));
    }
};

class SlideshowBackground: public GlobalComboBox {
public:
    SlideshowBackground() :
      GlobalComboBox("SlideshowBackground") {
        setLabel(QObject::tr("Type of background"));
        // use names from /etc/X11/rgb.txt
        addSelection("theme","");
        addSelection("black");
        addSelection("white");
        setHelpText(QObject::tr("This is the type of background for each "
                    "picture in single view mode."));
    }
};

class SlideshowDelay: public GlobalSpinBox {
public:
    SlideshowDelay():
        GlobalSpinBox("SlideshowDelay", 1, 600, 1) {
        setLabel(QObject::tr("Slideshow Delay"));
        setValue(5);
        setHelpText(QObject::tr("This is the number of seconds to display each "
                    "picture."));
    };
};


class GalleryConfigurationGroup: public VerticalConfigurationGroup,
                                 public TriggeredConfigurationGroup {
public:

    GalleryConfigurationGroup():
        VerticalConfigurationGroup(false),
        TriggeredConfigurationGroup(false) {
        
        setLabel(QObject::tr("MythGallery Settings"));
        setUseLabel(false);

        addChild(new MythGalleryDir());
        addChild(new MythGalleryImportDirs());
        addChild(new MythGalleryMoviePlayerCmd());

#ifdef OPENGL_SUPPORT
        
        SlideshowUseOpenGL* useOpenGL = new SlideshowUseOpenGL();
        addChild(useOpenGL);
        setTrigger(useOpenGL);
    
        ConfigurationGroup* openGLConfig = new VerticalConfigurationGroup(false);
        openGLConfig->addChild(new SlideshowOpenGLTransition());
        addTarget("1", openGLConfig);

        ConfigurationGroup* regularConfig = new VerticalConfigurationGroup(false);
        regularConfig->addChild(new SlideshowTransition());
        regularConfig->addChild(new SlideshowBackground());
        addTarget("0", regularConfig);

#else
        
        addChild(new SlideshowTransition());
        addChild(new SlideshowBackground());
        
#endif

        
        
        addChild(new SlideshowDelay());

    }

};


GallerySettings::GallerySettings()
{
    GalleryConfigurationGroup* config = new GalleryConfigurationGroup();
    addChild(config);
}

