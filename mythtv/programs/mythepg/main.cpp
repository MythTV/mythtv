#include <qapplication.h>
#include <qsqldatabase.h>
#include <stdlib.h>

#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"

#include "libmythtv/guidegrid.h"

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext();


    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!context->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    QString startchannel = context->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    if (a.argc() > 1)
    {
        startchannel = a.argv()[1];
    }

    context->LoadQtConfig();
    
    QString chanstr = RunProgramGuide(context, startchannel);

    int chan = 0;
    
    if (chanstr != QString::null)
        chan = atoi(chanstr.ascii());

    delete context;

    return chan;
}
