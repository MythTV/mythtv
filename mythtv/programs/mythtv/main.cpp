#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstring.h>
#include <unistd.h>
#include "tv.h"

#include "libmyth/mythcontext.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext();

    QString startChannel = context->GetSetting("DefaultTVChannel");
    if (startChannel == "")
        startChannel = "3";

    TV *tv = new TV(context, startChannel, 1, 2);
    tv->LiveTV();

    while (tv->GetState() == kState_None)
        usleep(1000);

    while (tv->GetState() != kState_None)
        usleep(1000);

    sleep(1);
    delete tv;

    exit(0);
}
