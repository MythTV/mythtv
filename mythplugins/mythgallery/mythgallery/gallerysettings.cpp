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
    settings->addChild(new SlideshowTransition());
    settings->addChild(new SlideshowDelay());
    addChild(settings);
}
