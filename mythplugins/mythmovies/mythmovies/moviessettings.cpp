#include <mythdirs.h>

#include "moviessettings.h"

static HostLineEdit *ZipCode()
{
    HostLineEdit *gc = new HostLineEdit("MythMovies.ZipCode");
    gc->setLabel(QObject::tr("Zip Code"));
    gc->setValue("00000");
    gc->setHelpText(QObject::tr("Enter your zip code here. "
                                "MythMovies will use it to find local theaters."));
    return gc;
}

static HostLineEdit *Radius()
{
    HostLineEdit *gc = new HostLineEdit("MythMovies.Radius");
    gc->setLabel(QObject::tr("Radius"));
    gc->setValue("20");
    gc->setHelpText(QObject::tr("Enter the radius (in miles) to search for theaters. "
                                "Numbers larger than 50 will be reduced to 50."));
    return gc;
}

static HostLineEdit *Grabber()
{
    HostLineEdit *gc = new HostLineEdit("MythMovies.Grabber");
    gc->setLabel(QObject::tr("Grabber:"));
    gc->setValue(QString("%1/bin/ignyte --zip %z --radius %r").arg(GetInstallPrefix()));
    gc->setHelpText(QObject::tr("This is the path to the data grabber to use."
                                "If you are in the United States, the default grabber "
                                "should be fine. If you are elsewhere, you'll need a "
                                "different grabber. %z will be replaced by the zip code"
                                "setting. %r will be replaced by the radius setting.")
                   );
    return gc;
}
MoviesSettings::MoviesSettings()
{
    VerticalConfigurationGroup *settings =
            new VerticalConfigurationGroup(false);
    settings->setLabel(QObject::tr("MythMovies Settings"));
    settings->addChild(ZipCode());
    settings->addChild(Radius());
    settings->addChild(Grabber());
    addChild(settings);
}
