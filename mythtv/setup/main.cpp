#include <qapplication.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdir.h>
#include <qfile.h>
#include <qstringlist.h>

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/videodev.h>

#include <iostream>

#include "libmyth/mythcontext.h"
#include "libmythtv/videosource.h"

using namespace std;

MythContext* context;
QSqlDatabase* db;

QString getResponse(const QString &query, const QString &def)
{
    cout << query;

    if (def != "")
    {
        cout << " [" << def << "]  ";
    }
    
    char response[80];
    cin.getline(response, 80);

    QString qresponse = response;

    if (qresponse == "")
        qresponse = def;

    return qresponse;
}

bool configXMLTV()
{
    bool ret = false;
    QSqlQuery result = db->exec("SELECT sourceid FROM videosource");

    if (!result.isActive() || result.numRowsAffected() < 1) {
        cerr << "No channel sources?\n";
        exit(1);
    }

    char *home = getenv("HOME");
    while (result.next())
    {
        VideoSource source(context);
        source.loadByID(db, result.value(0).toInt());
        QString name = source.byName("name")->getValue();
        QString xmltv_grabber = source.byName("xmltvgrabber")->getValue();
        QString filename = QString("%1/.mythtv/%2.xmltv").arg(home).arg(name);

        cout << "mythsetup will now run " << xmltv_grabber 
             << " --configure\n\n";
        sleep(1);

        QString command;

        if (xmltv_grabber == "tv_grab_de")
            command = xmltv_grabber + "--configure";
        else
            command = xmltv_grabber + " --configure --config-file " + filename;

        cout << "--------------- Start of XMLTV output ---------------" << endl;


        system(command);

        cout << "---------------- End of XMLTV output ----------------" << endl;

        if (xmltv_grabber == "tv_grab_uk" || xmltv_grabber == "tv_grab_de" ||
            xmltv_grabber == "tv_grab_sn")
            ret = true;
    }

    return ret;
}

void clearDB(void)
{
    QSqlQuery query;

    query.exec("DELETE FROM channel;");
    query.exec("DELETE FROM program;");
    query.exec("DELETE FROM singlerecord;");
    query.exec("DELETE FROM timeslotrecord;");
    query.exec("DELETE FROM capturecard;");
    query.exec("DELETE FROM videosource;");
    query.exec("DELETE FROM cardinput;");
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    context = new MythContext(false);

    db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!context->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    context->LoadQtConfig();

    char *home = getenv("HOME");
    QString fileprefix = QString(home) + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    if ("y" != getResponse("This will clear all program/channel/recording/card "
                           "info from the database.\nProceed?", "y"))
        exit(1);

    clearDB();

    CaptureCardEditor cce(context, db);
    cce.exec(db);

    VideoSourceEditor vse(context, db);
    vse.exec(db);

    CardInputEditor cie(context, db);
    cie.exec(db);

    bool need_manual = configXMLTV();

    cout << endl << endl;
    cout << "Now, please run 'mythfilldatabase' to populate the database\n";
    cout << "with channel information.\n";
    cout << endl;

    if (need_manual)
    {
        cout << "You _MUST_ run 'mythfilldatabase --manual the first time, "
             << "instead\n";
        cout << "of just 'mythfilldatabase'.  Your grabber does not provide\n";
        cout << "channel numbers, so you have to set them manually.\n";
    }

    return 0;
}
