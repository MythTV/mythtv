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

#include "libmyth/settings.h"

using namespace std;

Settings *globalsettings;
char installprefix[] = "/usr/local";


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

void getCaptureCards(void)
{
    QString inputstr;

    inputstr = getResponse("How many TV capture cards do you have?", "1");

    bool ok;
    int numcards = inputstr.toInt(&ok);

    while (!ok || numcards < 0)
    {
        cout << "\nERROR: " << inputstr 
             << " is not a valid number of cards\n\n";
        inputstr = getResponse("How many TV capture cards do you have?", "1");
        numcards = inputstr.toInt(&ok);
    }

    for (int i = 0; i < numcards; i++)
    {
        QString query = QString("Use which video device for card %1?").arg(i+1);

        QString videodev = getResponse(query, "/dev/video");
        
        QFile file(videodev);

        while (!file.exists()) 
        {
            cout << "\nERROR: " << videodev << " does not seem to exist\n\n";
            videodev = getResponse(query, "/dev/video");
           
            file.setName(videodev);
        }

        query = QString("Use which audio device for card %1?").arg(i+1);
        
        QString audiodev = getResponse(query, "/dev/dsp");

        file.setName(audiodev);
 
        while (!file.exists())
        {
            cout << "\nERROR: " << audiodev << " does not seem to exist\n\n";
            videodev = getResponse(query, "/dev/dsp");

            file.setName(audiodev);
        }

        cout << "Some cards can only record at a certain sampling rate,"
             << " like 32000Hz.\n";

        query = QString("Does this audio device need to record at a certain "
                        "rate? (0 for no)");

        QString audrate = getResponse(query, "0");

        int rate = audrate.toInt(&ok);

        while (!ok || rate < 0)
        {
            cout << "\nERROR: " << audrate
                 << " is not a valid sampling rate\n\n";
            audrate = getResponse(query, "0");
            rate = audrate.toInt(&ok);
        }

        cout << "card: " << i+1 << endl;
        cout << "  video: " << videodev << endl;
        cout << "  audio: " << audiodev << endl;

        QSqlQuery sqlquery;

        QString querystr;
        querystr = QString("INSERT INTO capturecard (videodevice,audiodevice,"
                           "audioratelimit) VALUES(\"%1\",\"%2\",%3);")
                           .arg(videodev).arg(audiodev).arg(rate);

        sqlquery.exec(querystr);
    }
}

void getSources(void)
{
    QString inputstr;
    QString query = "How many channel sources do you have? "
                    "(like cable, DirecTV, etc)";
    
    inputstr = getResponse(query, "1");
    
    bool ok;
    int numsources = inputstr.toInt(&ok);
    
    while (!ok || numsources < 0)
    {
        cout << "\nERROR: " << inputstr
             << " is not a valid number of channel sources\n\n";
        inputstr = getResponse(query, "1");
        numsources = inputstr.toInt(&ok);
    }

    for (int i = 0; i < numsources; i++)
    {
        query = QString("Please name input source #%1  ").arg(i+1);
        QString name = getResponse(query, "default");

        char *home = getenv("HOME");
        QString filename = QString("%1/.mythtv/%2.xmltv").arg(home).arg(name);

        QFile file(filename);
        if (file.exists())
        {
            file.remove();
        }

        QString xmltv_grabber = globalsettings->GetSetting("XMLTVGrab");

        cout << "\n\nConfiguring channel source #" << i+1 << endl;
        cout << "mythsetup will now run " << xmltv_grabber 
             << " --configure\n\n";
        sleep(1);

        QString command = xmltv_grabber + " --configure --config-file " + 
                          filename;

        cout << "--------------- Start of XMLTV output ---------------" << endl;


        system(command);

        cout << "---------------- End of XMLTV output ----------------" << endl;

        QSqlQuery sqlquery;

        QString querystr;
        querystr = QString("INSERT INTO videosource (name) "
                           "VALUES(\"%1\");").arg(name);

        sqlquery.exec(querystr);
    } 
}

QStringList probeCard(QString videodev)
{
    int videofd = open(videodev.ascii(), O_RDWR);
    QStringList retval;

    if (videofd <= 0)
    {
        cout << "Couldn't open " << videodev << " to probe its inputs.\n";
        return retval;
    }

    struct video_capability vidcap;
    memset(&vidcap, 0, sizeof(vidcap));
    ioctl(videofd, VIDIOCGCAP, &vidcap);

    for (int i = 0; i < vidcap.channels; i++)
    {
        struct video_channel test;
        memset(&test, 0, sizeof(test));
        test.channel = i;
        ioctl(videofd, VIDIOCGCHAN, &test);

        retval.append(test.name);
    }

    close(videofd);

    return retval;
}

struct Source
{
    int id;
    QString name;
};

void selectSource(int cardid, QString cardpath, QString input, 
                  QValueList<Source> &sourcelist)
{
    cout << "Please select a channel source to associate with:\n";
    cout << "  Card: " << cardid << " (" << cardpath << ")\n";
    cout << "  Input: " << input << endl << endl; 

    QValueList<Source>::Iterator it;
    int num = 0;
    for (it = sourcelist.begin(); it != sourcelist.end(); ++it)
    {
        cout << num+1 << ": " << (*it).name << endl;
        num++;
    }
    cout << num+1 << ": Nothing\n";

    QString defval = QString("%1").arg(num+1);
    QString ans = getResponse("Which channel source?", defval);

    bool ok;
    int sourcenum = ans.toInt(&ok);

    while (!ok || sourcenum <= 0 || sourcenum > num+1)
    {
        cout << "\nERROR: " << ans
             << " is not a valid menu selection\n\n";
        ans = getResponse("Which channel source?", defval);
        sourcenum = ans.toInt(&ok);
    }

    cout << endl;

    if (sourcenum == num + 1)
        return; 

    Source selected = sourcelist[sourcenum - 1];

    QSqlQuery sourcequery;
    QString querystr;
    querystr = QString("INSERT INTO cardinput (cardid,sourceid,inputname) "
                       "VALUES(%1,%2,\"%3\");").arg(cardid).arg(selected.id)
                                               .arg(input);

    sourcequery.exec(querystr);
}

void setupSources(void)
{
    cout << "\n\nNow, let's connect channel sources to inputs on the capture "
         << "cards.\n\n";

    QValueList<Source> sourcelist;

    QSqlQuery sourcequery;
    QString querystr = QString("SELECT sourceid,name FROM videosource "
                               "ORDER BY sourceid;");
    sourcequery.exec(querystr);

    if (sourcequery.isActive() && sourcequery.numRowsAffected() > 0)
    {
        while (sourcequery.next())
        {
            Source newsource;

            newsource.id = sourcequery.value(0).toInt();
            newsource.name = sourcequery.value(1).toString();

            sourcelist.append(newsource);
        }
    }

    QSqlQuery cardquery;
    querystr = QString("SELECT cardid,videodevice,audiodevice,cardtype"
                       " FROM capturecard ORDER BY cardid;");
    cardquery.exec(querystr);

    if (cardquery.isActive() && cardquery.numRowsAffected() > 0)
    {
        while (cardquery.next())
        {
            int idnum = cardquery.value(0).toInt();
            QString videodev = cardquery.value(1).toString();

            QStringList inputlist = probeCard(videodev); 
            QStringList::Iterator it;

            for (it = inputlist.begin(); it != inputlist.end(); ++it)
            {
                selectSource(idnum, videodev, *it, sourcelist); 
            }
        }
    }
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
    QApplication a(argc, argv, false);

    globalsettings = new Settings;
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);
    globalsettings->LoadSettingsFiles("settings.txt", installprefix);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    db->setDatabaseName(globalsettings->GetSetting("DBName"));
    db->setUserName(globalsettings->GetSetting("DBUserName"));
    db->setPassword(globalsettings->GetSetting("DBPassword"));
    db->setHostName(globalsettings->GetSetting("DBHostName"));

    if (!db->open())
    {
        printf("couldn't open db\n");
        return -1;
    }

    char *home = getenv("HOME");
    QString fileprefix = QString(home) + "/.mythtv";

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    clearDB();

    getCaptureCards();

    getSources();

    setupSources();

    cout << "Now, please run mythfilldatabase to populate the database with "
            "info.\n";

    return 0;
}
