#include <qapplication.h>
#include <qsqldatabase.h>

#include "guidegrid.h"

int main(int argc, char **argv)
{
    QApplication a(argc, argv);


    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    db->setDatabaseName("mythconverg");
    db->setUserName("mythtv");
    db->setPassword("mythtv");
    db->setHostName("localhost");

    if (!db->open())
        printf("couldn't open db\n");

    int startchannel = 0;

    if (a.argc() > 1)
    {
        startchannel = atoi(a.argv()[1]);
    }

    GuideGrid gg(startchannel);
    gg.setGeometry(0, 0, 800, 600);
    gg.setFixedWidth(800);
    gg.setFixedHeight(600);
    a.setMainWidget(&gg);

    gg.showFullScreen();
    //gg.show();

    gg.setActiveWindow();
    gg.raise();
    gg.setFocus();

    a.exec();

    int chan = gg.getLastChannel();
    return chan;
}
