#include <qapplication.h>
#include <qsqldatabase.h>
#include <stdlib.h>

#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"

#include "libmythtv/progfind.h"

MythContext *gContext;

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    gContext = new MythContext();

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    gContext->LoadQtConfig();
    
    RunProgramFind();

    delete gContext;

    return 0;
}
