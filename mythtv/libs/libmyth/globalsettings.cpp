#include "globalsettings.h"
#include <qfile.h>
#include <qdialog.h>
#include <qcursor.h>

// xxx
#include <qsqldatabase.h>
#include <qapplication.h>

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


void GlobalSettings::exec(QSqlDatabase* db) {
    //     int screenwidth, screenheight;
    //     float wmult, hmult;
    //     context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    QDialog* dialog = dialogWidget(NULL);
    //     dialog->setGeometry(0, 0, screenwidth, screenheight);
    //     dialog->setFixedSize(QSize(screenwidth, screenheight));
    //     dialog->setCursor(QCursor(Qt::BlankCursor));
    dialog->show();

    if (dialog->exec() == QDialog::Accepted)
        save(db);

    delete dialog;
}

// int main(int argc, char *argv[]) {
//     QApplication app(argc, argv);
//     MythContext* context = new MythContext;

//     QSqlDatabase* db = QSqlDatabase::addDatabase("QMYSQL3", "recordingprofiletest"
//                                                  );
//     db->setDatabaseName("mythconverg");
//     db->setUserName("mythtv");
//     db->setPassword("mythtv");
//     db->setHostName("localhost");
//     db->open();

//     GlobalSettings settings;
//     settings.exec(db);

//     return 0;
// }
