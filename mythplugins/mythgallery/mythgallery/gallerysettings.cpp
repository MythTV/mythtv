#include <mythtv/mythcontext.h>

#include "gallerysettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

// General Settings

class MythGalleryDir: public LineEditSetting, public GlobalSetting {
public:
    MythGalleryDir():
        GlobalSetting("GalleryDir") {
        setLabel("Directory that holds images");
        setValue("/var/lib/pictures");
        setHelpText("This directory must exist and "
                    "MythGallery needs to have read permission.");
    };
};

class MythGalleryImportDirs: public LineEditSetting, public GlobalSetting {
public:
    MythGalleryImportDirs():
        GlobalSetting("GalleryImportDirs") {
        setLabel("Paths to import images from");
        setValue("/mnt/cdrom:/mnt/camera");
        setHelpText("This is a colon separated list of paths.  If the path "
                    "in the list is a directory, its contents will be copied. "
                    "If it is an executable, it will be run.");
    };
};

class SlideshowTransition: public ComboBoxSetting, public GlobalSetting {
public:
    SlideshowTransition() : ComboBoxSetting(true),
      GlobalSetting("SlideshowTransition") {
        setLabel("Type of transition");
        addSelection("none");
        addSelection("fade"); 
        addSelection("wipe");
        setHelpText("This is the type of transition used between pictures "
                    " in slideshow mode.");
    }
};

class SlideshowBackground: public ComboBoxSetting, public GlobalSetting {
public:
    SlideshowBackground() : ComboBoxSetting(true),
      GlobalSetting("SlideshowBackground") {
        setLabel("Type of background");
        // use names from /etc/X11/rgb.txt
        addSelection("theme","");
        addSelection("black");
        addSelection("white");
        setHelpText("This is the type of background for each picture "
                    " in single view mode.");
    }
};

class SlideshowDelay: public SpinBoxSetting, public GlobalSetting {
public:
    SlideshowDelay():
        SpinBoxSetting(1,600,1) ,
        GlobalSetting("SlideshowDelay") {
        setValue(5);
        setHelpText("This is the number of seconds to display each picture.");
    };
};


GallerySettings::GallerySettings()
{
    VerticalConfigurationGroup* settings = new VerticalConfigurationGroup(false);
    settings->setLabel("MythGallery Settings");
    settings->addChild(new MythGalleryDir());
    settings->addChild(new MythGalleryImportDirs());
    settings->addChild(new SlideshowTransition());
    settings->addChild(new SlideshowBackground());
    settings->addChild(new SlideshowDelay());
    addChild(settings);
}

