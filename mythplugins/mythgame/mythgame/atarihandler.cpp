#include <string>
#include <qdir.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include "atarihandler.h"
#include "atarirominfo.h"
#include "atarisettingsdlg.h"

#include <iostream>

#include <mythtv/mythdialogs.h>

using namespace std;

AtariHandler* AtariHandler::pInstance = 0;

AtariHandler* AtariHandler::getHandler()
{
    if(!pInstance)
    {
        pInstance = new AtariHandler();
    }
    return pInstance;
}

void AtariHandler::start_game(RomInfo* romdata)
{
    QString exec = gContext->GetSetting(QString("%1Binary").arg(systemname)) + " " +
                   "\"" + gContext->GetSetting(QString("%1RomLocation").arg(systemname)) + "/" + 
                   romdata->Romname() + "\"";
    cout << exec << endl;
    
    // Run the emulator and wait for it to terminate.      
    FILE* command = popen(exec, "w");
    pclose(command);
}

void AtariHandler::edit_settings(RomInfo* romdata)
{
    romdata = romdata;

    AtariSettingsDlg settingsdlg(romdata->Romname().latin1());
    settingsdlg.exec();
}

void AtariHandler::edit_system_settings(RomInfo* romdata)
{
    romdata = romdata;

    AtariSettingsDlg settingsdlg("default");
    settingsdlg.exec();
}

void AtariHandler::processGames()
{
    QString thequery;

    MSqlQuery query(MSqlQuery::InitCon());

    // Remove all metadata entries from the tables, all correct values will be
    // added as they are found.  This is done so that entries that may no longer be
    // available or valid are removed each time the game list is remade.
    thequery = "DELETE FROM gamemetadata WHERE system = \"%1\";";
    thequery.arg(systemname);
    query.exec(thequery);

    // Search the rom dir for valid new roms.
    QDir RomDir(gContext->GetSetting(QString("%1RomLocation").arg(systemname)));
    const QFileInfoList* List = RomDir.entryInfoList();

    if (!List)
        return;

    MythProgressDialog pdial(QObject::tr("Looking for Atari games..."), List->count());
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

RomInfo* AtariHandler::create_rominfo(RomInfo* parent)
{
    return new AtariRomInfo(*parent);
}

bool AtariHandler::IsValidRom(QString Path)
{
    // Anything better out there?
    return (Path.right(4) == ".bin");;
}

QString AtariHandler::GetGameName(QString Path)
{
    // TODO: read stella.pro
    return QString();
}

void AtariHandler::GetMetadata(QString GameName, QString* Genre, int* Year)
{
    // ???
    return;
}
