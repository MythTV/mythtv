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
    {
        printf("couldn't open db\n");
        return -1;
    }

    int startchannel = 0;

    if (a.argc() > 1)
    {
        startchannel = atoi(a.argv()[1]);
    }

    GuideGrid gg(startchannel);

    gg.exec();

    int chan = gg.getLastChannel();
    return chan;
}
