#include <iostream>
#include <qfile.h>
#include <qsqldatabase.h>
#include "nesrominfo.h"

bool NesRomInfo::FindImage(QString type, QString* result)
{
    bool retval = false;

    if (type != "screenshot")
        return false;
    
    // Try to match the GoodNES name against the title table in order to get an image
    // index that hopefully matches up with the game.
    
    QString thequery; 
    thequery = QString("SELECT screenshot, description FROM nestitle WHERE "
                       "MATCH(description) AGAINST ('%1');").arg(Gamename());
    
    QSqlDatabase* db = QSqlDatabase::database();
    QSqlQuery query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        // Take first entry since that will be the most relevant match.
        query.first();
        QString current = query.value(0).toString();
        *result = gContext->GetSetting("NesScreensLocation") + "/" + current + ".gif";
        retval = QFile::exists(*result);
        if (!retval)
        {
            // Look for file with the same name as the Game
            current = query.value(1).toString();
            *result = gContext->GetSetting("NesScreensLocation") + "/" + current + ".gif";
            retval = QFile::exists(*result);
        }
        else if (!retval)
        {
            // Now just try to match the game name
            *result = gContext->GetSetting("NesScreensLocation") + "/" + Gamename() + ".gif";
            retval = QFile::exists(*result);
        }
        else if (!retval)
        {
            // Now just try to match the game name
            *result = gContext->GetSetting("NesScreensLocation") + "/" + Romname() + ".gif";
            retval = QFile::exists(*result);
        }
    }

    return retval;
}
