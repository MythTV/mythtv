#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstring.h>
#include <unistd.h>
#include "tv.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString startChannel = "3";

    TV *tv = new TV(startChannel);
    tv->LiveTV();

    while (tv->GetState() == kState_None)
        usleep(1000);

    while (tv->GetState() != kState_None)
        usleep(1000);

    sleep(1);
    printf("shutting down\n");
    delete tv;
    printf("Goodbye\n");

    exit(0);
}
