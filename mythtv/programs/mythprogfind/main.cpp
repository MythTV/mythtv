#include <qapplication.h>
#include <qsqldatabase.h>
#include <stdlib.h>

#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"

#include "libmythtv/progfind.h"

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

    context->LoadQtConfig();
    
    RunProgramFind(context);

    delete context;

    return 0;
}
