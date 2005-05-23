#include <string>
#include <qdir.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include "odyssey2handler.h"
#include "odyssey2rominfo.h"
#include "odyssey2settingsdlg.h"

#include <iostream>

#include <mythtv/mythdialogs.h>

using namespace std;

Odyssey2Handler* Odyssey2Handler::pInstance = 0;

Odyssey2Handler* Odyssey2Handler::getHandler()
{
    if(!pInstance)
    {
        pInstance = new Odyssey2Handler();
    }
    return pInstance;
}

void Odyssey2Handler::start_game(RomInfo* romdata)
{
    QString exec = gContext->GetSetting(QString("%1Binary").arg(systemname)) + " " +
                   "\"" + gContext->GetSetting(QString("%1RomLocation").arg(systemname)) + "/" + 
                   romdata->Romname() + "\"";
    cout << exec << endl;
    
    // Run the emulator and wait for it to terminate.      
    FILE* command = popen(exec, "w");
    pclose(command);
}

void Odyssey2Handler::edit_settings(RomInfo* romdata)
{
    romdata = romdata;

    Odyssey2SettingsDlg settingsdlg(romdata->Romname().latin1());
    settingsdlg.exec();
}

void Odyssey2Handler::edit_system_settings(RomInfo* romdata)
{
    romdata = romdata;

    Odyssey2SettingsDlg settingsdlg("default");
    settingsdlg.exec();
}

void Odyssey2Handler::processGames()
{
    // Remove all metadata entries from the tables, all correct values will be
    // added as they are found.  This is done so that entries that may no longer be
    // available or valid are removed each time the game list is remade.
    QString thequery;
    thequery = "DELETE FROM gamemetadata WHERE system = \"%1\";";
    thequery.arg(systemname);
  
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);

    // Search the rom dir for valid new roms.
    QDir RomDir(gContext->GetSetting(QString("%1RomLocation").arg(systemname)));
    const QFileInfoList* List = RomDir.entryInfoList();

    if (!List)
        return;

    MythProgressDialog pdial(QObject::tr("Looking for Odyssey2 games..."), List->count());
    int progress = 0;

    for (QFileInfoListIterator it(*List); it; ++it)
    {
        pdial.setProgress(progress);
        progress++;

        QFileInfo Info(*it.current());
        if (IsValidRom(Info.filePath()))
        {
            QString GameName = GetGameName(Info.filePath());
            if (GameName.isNull())
            {
                GameName = Info.fileName();
            }
            cout << GameName << endl;

            QString Genre(QObject::tr("Unknown"));
            int Year = 0;
            GetMetadata(GameName, &Genre, &Year);
            
            // Put the game into the database.
            thequery = QString("INSERT INTO gamemetadata "
                               "(system, romname, gamename, genre, year) "
                               "VALUES (\"%1\", \"%1\", \"%2\", \"%3\", %4);")
                               .arg(systemname)
                               .arg(Info.fileName().latin1())
                               .arg(GameName.latin1()).arg(Genre.latin1())
                               .arg(Year);
            query.exec(thequery);
        }
        else
        {
            // Unknown type of file.
        }
    }
   
    pdial.Close();
}

RomInfo* Odyssey2Handler::create_rominfo(RomInfo* parent)
{
    return new Odyssey2RomInfo(*parent);
}

bool Odyssey2Handler::IsValidRom(QString Path)
{
    // Anything better out there?
    return (Path.right(4) == ".bin");
}

QString Odyssey2Handler::GetGameName(QString)
{
    // TODO: read stella.pro
    return QString();
}

void Odyssey2Handler::GetMetadata(QString, QString*, int*)
{
    // ???
    return;
}
