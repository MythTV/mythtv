#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>

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
    GlobalSetting("Theme") {

    setLabel("Theme");

    QDir themes(PREFIX"/share/mythtv/themes");
    themes.setFilter(QDir::Dirs);
    themes.setSorting(QDir::Name | QDir::IgnoreCase);

    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);
    if (!fil)
        return;

    QFileInfoListIterator it( *fil );
    QFileInfo *theme;

    for( ; it.current() != 0 ; ++it ) {
        theme = it.current();

        if ( theme->fileName() == "." || theme->fileName() == ".." )
            continue;

        QFileInfo preview(theme->absFilePath() + "/preview.jpg");
        QFileInfo xml(theme->absFilePath() + "/theme.xml");

        if (!preview.exists() || !xml.exists()) {
            //cout << theme->absFilePath() << " doesn't look like a theme\n";
            continue;
        }

        QImage* previewImage = new QImage(preview.absFilePath());
        if (previewImage->width() == 0 || previewImage->height() == 0) {
            cout << "Problem reading theme preview image " << preview.dirPath() << endl;
            continue;
        }

        addImageSelection(theme->fileName(), previewImage);
    }
}
