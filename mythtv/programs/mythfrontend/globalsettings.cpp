#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>

const char* AudioOutputDevice::paths[] = { "/dev/dsp",
                                           "/dev/dsp1",
                                           "/dev/dsp2",
                                           "/dev/sound/dsp" };

AudioOutputDevice::AudioOutputDevice():
    ComboBoxSetting(true),
    GlobalSetting("AudioOutputDevice") {

    setLabel("Audio output device");
    for(unsigned int i = 0; i < sizeof(paths) / sizeof(char*); ++i) {
        if (QFile(paths[i]).exists())
            addSelection(paths[i]);
    }
}

ThemeSelector::ThemeSelector():
    PathSetting(true), GlobalSetting("WhichTheme") {

    setLabel("Theme");

    QDir themes(PREFIX"/share/mythtv/themes");
    themes.setFilter(QDir::Dirs);
    const QFileInfoList *fil = themes.entryInfoList();
    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    while ((theme = it.current()) != 0)
        addSelection(theme->fileName());
}
