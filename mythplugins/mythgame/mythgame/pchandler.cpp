#include <iostream>
#include "pchandler.h"

#include <qobject.h>
#include <qsqldatabase.h>

#include <sys/types.h>
#include <unistd.h>

#include <mythtv/mythcontext.h>
#include <qfile.h>
#include <qdom.h>
#include <string>

#include "pcsettingsdlg.h"

using namespace std;

PCHandler::~PCHandler()
{
}    

void PCHandler::processGames()
{
    QString thequery;
    QSqlDatabase* db = QSqlDatabase::database();

    thequery = "DELETE FROM gamemetadata WHERE system = \"PC\";";
    db->exec(thequery);

    QString GameDoc = gContext->GetSetting("PCGameList");
    if(!QFile::exists(GameDoc))
        return;

    QDomDocument doc;

    QFile f(GameDoc);

    if (!f.open(IO_ReadOnly))
    {
        cout << "Can't open: " << GameDoc << endl;
        return;
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        cout << "Error parsing: " << GameDoc << endl;
        cout << "at line: " << errorLine << "  column: " << errorColumn << endl;
        cout << errorMsg << endl;
        f.close();
        return;
    }

    f.close();
    
    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "game")
            {
                QString GameName = "";
                QString Command = "";
                QString Genre = "";
                int Year = 0;
                QDomNode i = e.firstChild();
                while (!i.isNull())
                {
                    QDomElement setting = i.toElement();
                    if (!setting.isNull())
                    {
                        if (setting.tagName() == "name")
                            GameName = setting.text();
                        else if (setting.tagName() == "command")
                            Command = setting.text();
                        else if (setting.tagName() == "genre")
                            Genre = setting.text();
                        else if (setting.tagName() == "year")
                            Year = setting.text().toInt();
                    }
                    i = i.nextSibling();
                }

                if(GameName != "" && Command != "")
                {
                    thequery = QString("INSERT INTO gamemetadata "
                                   "(system, romname, gamename, genre, year) "
                                   "VALUES (\"PC\", \"%1\", \"%2\", \"%3\", %4);")
                                   .arg(Command.latin1())
                                   .arg(GameName.latin1()).arg(Genre.latin1())
                                   .arg(Year);
                    db->exec(thequery);
                }
            }
        }
        n = n.nextSibling();
    }
}

void PCHandler::start_game(RomInfo * romdata)
{
    // Run the game and wait for it to terminate.
    FILE* command = popen(romdata->Romname(), "w");
    pclose(command);
}

void PCHandler::edit_settings(MythMainWindow *parent,RomInfo * romdata)
{
    PCRomInfo *pcdata = dynamic_cast<PCRomInfo*>(romdata);
    PCSettingsDlg settingsdlg(parent, "gamesettings");
    QString ImageFile;
    if(pcdata->FindImage("screenshot", &ImageFile))
        settingsdlg.SetScreenshot(ImageFile);
    settingsdlg.Show(pcdata);
}

void PCHandler::edit_system_settings(MythMainWindow *parent,RomInfo * romdata)
{
    romdata = romdata;
    PCSettingsDlg settingsDlg(parent, "pcsettings", true);
    settingsDlg.Show(NULL);    
}

PCHandler* PCHandler::getHandler(void)
{
    if(!pInstance)
    {
        pInstance = new PCHandler();
    }
    return pInstance;
}

PCRomInfo* PCHandler::create_rominfo(RomInfo *parent)
{
    return new PCRomInfo(*parent);
} 

PCHandler* PCHandler::pInstance = 0;

