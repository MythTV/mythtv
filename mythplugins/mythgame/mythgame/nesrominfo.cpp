#include <iostream>
#include <qfile.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "nesrominfo.h"

bool NesRomInfo::FindImage(QString type, QString* result)
{
    if (type != "screenshot")
        return false;

    QString screenLocation = gContext->GetSetting("NesScreensLocation");

    // First, before looking at the DB, just try and see if a screenshot matches the rom name
    *result = screenLocation + "/" + Romname() + ".gif";
    if (QFile::exists(*result)) 
        return true;

    // Now just try to match the game name
    *result = screenLocation + "/" + Gamename() + ".gif";
    if (QFile::exists(*result)) 
        return true;
    
    // Try to match the GoodNES name against the title table in order to get an image
    // index that hopefully matches up with the game.
    
    QString thequery; 
    thequery = QString("SELECT screenshot, description FROM nestitle WHERE "
                       "MATCH(description) AGAINST ('%1');").arg(Gamename());
    
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);

    if (query.isActive() && query.size() > 0)
    {
        // Take first entry since that will be the most relevant match.
        query.first();
        QString current = query.value(0).toString();
        *result = screenLocation + "/" + current + ".gif";
        if (QFile::exists(*result)) 
            return true;

        // Look for file with the same name as the Game
        current = query.value(1).toString();
        *result = screenLocation + "/" + current + ".gif";
        if (QFile::exists(*result)) 
            return true;
    }

    return false;
}
