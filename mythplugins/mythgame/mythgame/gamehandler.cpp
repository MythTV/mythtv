#include "gamehandler.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <iostream>
#include <qdir.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>


using namespace std;

GameHandler::~GameHandler()
{
}

static QPtrList<GameHandler> *handlers = 0;

bool existsHandler(const QString name)
{
    GameHandler *handler = handlers->first();
    while(handler)
    {
        if (handler->SystemName() == name)
        {
            return TRUE;
        }
        
        handler = handlers->next();
    }

    return FALSE;
}

static void checkHandlers(void)
{
    // If a handlers list doesn't currently exist create one. Otherwise
    // clear the existing list so that we can regenerate a new one.

    if (! handlers)
    {   
        handlers = new QPtrList<GameHandler>;
    }
    else {
        handlers->clear();
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT DISTINCT playername FROM gameplayers WHERE playername <> '';");
    while (query.next()) 
    {   
        QString name = query.value(0).toString();
        GameHandler::registerHandler(GameHandler::newHandler(name));
    }   

}


GameHandler* GameHandler::getHandler(uint i)
{
    checkHandlers();
    return handlers->at(i);
}

void GameHandler::updateSettings(GameHandler *handler)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT rompath, workingpath, commandline, screenshots, gameplayerid, gametype, extensions  FROM gameplayers WHERE playername = \"" + handler->SystemName() + "\";");

    query.next();
    handler->rompath = query.value(0).toString();
    handler->workingpath = query.value(1).toString();
    handler->commandline = query.value(2).toString();
    handler->screenshots = query.value(3).toString();
    handler->gameplayerid = query.value(4).toInt();
    handler->gametype = query.value(5).toString();
    handler->validextensions = QStringList::split(",", query.value(6).toString().stripWhiteSpace().remove(" "));

}

GameHandler* GameHandler::newInstance = 0;

GameHandler* GameHandler::newHandler(QString name)
{
    newInstance = new GameHandler();
    newInstance->systemname = name;

    updateSettings(newInstance);

    return newInstance;
}

uint GameHandler::count(void)
{
    checkHandlers();
    return handlers->count();
}

bool GameHandler::validRom(QString RomName,GameHandler *handler)
{
    // Add proper checks here
    return TRUE;
}

void GameHandler::GetMetadata(GameHandler *handler,QString GameName, QString *Genre, int *Year)
{
    // Set some default return values
    //Genre = QObject::tr("Unknown");
    //Year = 0;

    if (handler->GameType() == "MAME")
    {
    }
    else if (handler->GameType() == "SNES")
    {
    }
    else if (handler->GameType() == "NES")
    {
    }

    
}

void GameHandler::buildFileList(QString directory,
                                GameHandler *handler, MSqlQuery *query)
{
    QString thequery;
    QDir RomDir(directory);
    const QFileInfoList* List = RomDir.entryInfoList();

    for (QFileInfoListIterator it(*List); it; ++it)
    {
        QFileInfo Info(*it.current());
        QString GameName = Info.fileName();

        if (GameName == "." ||
            GameName == "..")
        {
            continue;
        }

        if (Info.isDir())
        {
            buildFileList(Info.filePath(), handler, query);
            continue;
        }
        else
        {
            if (handler->validextensions.count() > 0)
            {
                QRegExp r;

                r.setPattern("^" + Info.extension() + "$");
                r.setCaseSensitive(false);
                QStringList result = handler->validextensions.grep(r);
                if (result.isEmpty()) {
                    continue;
                }
            }

            if (validRom(Info.filePath(),handler))
            {
                cout << "Found Rom : " << GameName << endl;

                // if valid rom then insert into db
                QString Genre(QObject::tr("Unknown"));
                int Year = 0;
                //GetMetadata(GameName, &Genre, &Year);

                // Put the game into the database.
                thequery = QString("INSERT INTO gamemetadata "
                           "(system, romname, gamename, genre, year, gametype, rompath) "
                           "VALUES (\"%1\", \"%1\", \"%2\", \"%3\", %4, \"%5\", \"%6\");")
                           .arg(handler->SystemName())
                           .arg(Info.fileName().latin1())
                           .arg(GameName.latin1())
                           .arg(Genre.latin1())
                           .arg(Year)
                           .arg(handler->GameType())
                           .arg(Info.dirPath().latin1());
                query->exec(thequery);
            }
            // else skip it
        }
    }
}

void GameHandler::processGames(GameHandler *handler)
{
    QString thequery;
    MSqlQuery query(MSqlQuery::InitCon());

    // Remove all metadata entries from the tables, all correct values will be
    // added as they are found.  This is done so that entries that may no longer be
    // available or valid are removed each time the game list is remade.

    // If we are not going to call processGames from anywhere but processAllGames then the following
    // Deletion can likely be removed and processGames and processAllGames merged together.

    thequery = "DELETE FROM gamemetadata WHERE system = \"" + handler->SystemName() + "\";";
    query.exec(thequery);

    MythProgressDialog pdial(QObject::tr("Looking for " + handler->SystemName() + " game(s)..."), 100);
    pdial.setProgress(50);

    if (handler->GameType() == "PC") 
    {
        thequery = QString("INSERT INTO gamemetadata "
                           "(system, romname, gamename, genre, year, gametype) "
                           "VALUES (\"%1\", \"%2\", \"%3\", \"Unknown\", 0 , \"%4\");")
                           .arg(handler->SystemName())
                           .arg(handler->SystemName())
                           .arg(handler->SystemName())
                           .arg(handler->GameType());

        query.exec(thequery);

        cout << " PC Game " << handler->SystemName() << endl;
    }
    else
    {   
        QDir d(handler->SystemRomPath());
        if (d.exists())
        {
            buildFileList(handler->SystemRomPath(),handler,&query);
        }
        else
        {
            cout << "Rom Path does not exist : " << handler->SystemRomPath() << endl;
        }
    }

    pdial.Close();
}

void GameHandler::processAllGames(void)
{
    checkHandlers();

    // Since we are processing all game handlers with a freshly built handler list
    // we can safely remove all existing gamemetadata first so that we don't have any 
    // left overs from removed or renamed handlers.
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = "DELETE FROM gamemetadata;";
    query.exec(thequery);

    GameHandler *handler = handlers->first();
    while(handler)
    {
	updateSettings(handler);
        handler->processGames(handler);
        handler = handlers->next();
    }
}

GameHandler* GameHandler::GetHandler(RomInfo *rominfo)
{
    if (!rominfo)
        return NULL;
    checkHandlers();
    GameHandler *handler = handlers->first();
    while(handler)
    {   
        if(rominfo->System() == handler->SystemName())
        {   
            return handler;
        }
        handler = handlers->next();
    }
    return handler;
}

void GameHandler::Launchgame(RomInfo *romdata)
{
    GameHandler *handler;
    if((handler = GetHandler(romdata)))
    {
       QString exec = handler->SystemCmdLine();
       QString arg = " \"" + handler->SystemRomPath() + 
                     "/" + romdata->Romname() + "\"";

       if (handler->GameType() != "PC")
       {
           QString arg = " \"" + handler->SystemRomPath() +
                         "/" + romdata->Romname() + "\"";

           // If they specified a %s in the commandline place the romname
           // in that location, otherwise tack it on to the end of
           // the command.
           if (exec.contains("%s"))
           {
               exec = exec.replace(QRegExp("%s"),arg);
           }
           else
           {
               exec = exec + " \"" + 
                      handler->SystemRomPath() + "/" +
                      romdata->Romname() + "\"";
           }
       }

       QString savedir = QDir::currentDirPath ();
       QDir d;
       if (handler->SystemWorkingPath()) {
           if (!d.cd(handler->SystemWorkingPath()))
           {
               cout << "Failed to change to specified Working Directory : " << handler->SystemWorkingPath() << endl;
           }
       }
       
       cout << "Launching Game : " << handler->SystemName() << " : " << exec << " : " << endl;

       myth_system(exec);

       (void)d.cd(savedir);      

    }
}

RomInfo* GameHandler::CreateRomInfo(RomInfo* parent)
{
    GameHandler *handler;
    if((handler = GetHandler(parent)))
        return new RomInfo(*parent);
    return NULL;
}

void GameHandler::registerHandler(GameHandler *handler)
{
    handlers->append(handler);
}

