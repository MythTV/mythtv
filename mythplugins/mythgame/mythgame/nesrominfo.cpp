#include <iostream>
#include <qfile.h>
#include <qsqldatabase.h>
#include "nesrominfo.h"

bool NesRomInfo::FindImage(QString type, QString* result)
{
    if (type != "screenshot")
        return false;
    
    // Try to match the GoodNES name against the title table in order to get an image
    // index that hopefully matches up with the game.
    
    QString thequery; 
    thequery = QString("SELECT screenshot FROM nestitle WHERE "
                       "MATCH(description) AGAINST ('%1');").arg(Gamename());
    
    QSqlDatabase* db = QSqlDatabase::database();
    QSqlQuery query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        // Take first entry since that will be the most relevant match.
        query.first();
        QString current = query.value(0).toString();
        *result = gContext->GetSetting("NesScreensLocation") + "/" + current + ".gif";
        return QFile::exists(*result);
    }
    else
    {
        return false;
    }
}
