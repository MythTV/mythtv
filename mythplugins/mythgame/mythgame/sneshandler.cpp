#include <fstream>
#include <iostream>
#include "sneshandler.h"
#include "snessettingsdlg.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <qdir.h>

#include <sys/types.h>
#include <unistd.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>

#include <string>
#include <math.h>

using namespace std;

#define ONEMBIT 131072
#define ONEMBYTE 1048576

SnesHandler::~SnesHandler()
{
}

bool SnesHandler::IsSnesRom(QString Path, RomHeader* Header, bool bVerifyChecksum)
{
    bool SnesRom = false;
    unsigned int bytesRead = 0;
    unsigned int start = 0;
    bool Verified = false;  //this holds the value for the Rom Header verification
    unsigned int Mbit, Low, High, LowTotal, HighTotal, ChkFound, Mult;

    // See if the file is a zip file first
    unzFile zf;
    if ((zf = unzOpen(Path)))
    {
        // TODO Does this loop check the last file?
        int FoundFile;
        for (FoundFile = unzGoToFirstFile(zf); FoundFile == UNZ_OK;
             FoundFile = unzGoToNextFile(zf))
        {
            if (unzOpenCurrentFile(zf) == UNZ_OK)
            {
                bytesRead = 0;
                Verified = VerifyZipRomHeader(zf, 32704, bytesRead, Header);
                if(!Verified)
                {
                    start = 512;
                    Verified = VerifyZipRomHeader(zf, 33216, bytesRead, Header); 
                }
                if(!Verified)
                {
                    start = 0;
                    Verified = VerifyZipRomHeader(zf, 65472, bytesRead, Header);
                }
                if(!Verified)
                {
                    start = 512;
                    Verified = VerifyZipRomHeader(zf, 65984, bytesRead, Header);
                }
                unzCloseCurrentFile(zf);
                if(!bVerifyChecksum)
                    return Verified;
                if (Verified)
                {
                    unz_file_info ZipFileInfo;
                    unzGetCurrentFileInfo(zf,&ZipFileInfo,NULL,0,NULL,0,NULL,0);
                    Mbit = (int) ZipFileInfo.uncompressed_size / ONEMBIT;
                    Low = (unsigned int)pow(2.0, (int)(log10((float)Mbit) / log10(2.0)));
                    High = Mbit - Low;
                    HighTotal = LowTotal = 0;
                    unsigned char current;
                    if (unzOpenCurrentFile(zf) == UNZ_OK)
                    {
                        for(unsigned int i = 0;i < start;i++)
                             unzReadCurrentFile(zf, &current, 1);       
                        for(unsigned int i = 0;i < Low * ONEMBIT; i++)
                        {
                            unzReadCurrentFile(zf, &current, 1);
                            LowTotal+= current;
                        }
                        for(unsigned int i = Low * ONEMBIT;i < Mbit * ONEMBIT;i++)
                        {
                            unzReadCurrentFile(zf, &current, 1);
                            HighTotal+= current;
                        }
                        Mult = 0;
                        if(High)
                            Mult = Low / High;
                        ChkFound = (unsigned short)(LowTotal + HighTotal * Mult);
                        if(Header->RomChecksum == ChkFound)
                            SnesRom = true;
                    }                  
                }
            }
        }
        unzClose(zf);
    }
    else
    {
        // Normal file
        QFile qf(Path);
        if (qf.open(IO_ReadOnly))
        {
            FILE* pFile;
            if((pFile = fopen(Path, "rb")))
            {
                Verified = VerifyRomHeader(pFile,32704,Header);
                if(!Verified)
                    Verified = VerifyRomHeader(pFile,65472,Header);
                if(!Verified)
                {
                    start = 512;
                    Verified = VerifyRomHeader(pFile,33216,Header);
                }
                if(!Verified)
                    Verified = VerifyRomHeader(pFile,65984,Header);
                if(!bVerifyChecksum)
                    return Verified;
                if(Verified)
                {
                    fseek(pFile,0,SEEK_END);
                    Mbit = (int) ftell(pFile) / ONEMBIT;
                    Low = (unsigned int)pow(2.0, (int)(log10((float)Mbit) / log10(2.0)) );
                    High = Mbit - Low;
                    HighTotal = LowTotal = 0;
                    unsigned char current;
                    fseek(pFile,start,SEEK_SET);
                    for(unsigned int i = 0;i < Low * ONEMBIT; i++)
                    {
                        fread(&current,1,1,pFile);
                        LowTotal+= current;
                    }
                    for(unsigned int i = Low * ONEMBIT;i < Mbit * ONEMBIT;i++)
                    {
                        fread(&current,1,1,pFile);
                        HighTotal+= current;
                    }
                    Mult = 0;
                    if(High)
                        Mult = Low / High;
                    ChkFound = (unsigned short)(LowTotal + HighTotal * Mult);
                    if(Header->RomChecksum == ChkFound)
                        SnesRom = true;
                }
            }
            fclose(pFile);
        }
    }

    return  SnesRom;
}

void SnesHandler::processGames(bool bValidate)
{
    if(bValidate)
    {
        processGames();
    }
    else
    {
        QString thequery;
        MSqlQuery query(MSqlQuery::InitCon());
        
        query.exec("DELETE FROM gamemetadata WHERE system = \"Snes\";");

        // Search the rom dir for valid new roms.
        QDir RomDir(gContext->GetSetting("SnesRomLocation"));
        const QFileInfoList* List = RomDir.entryInfoList();

        if (!List)
            return;

        RomHeader RHeader;

        QStringList rom_extensions;
        rom_extensions.append("zip");
        rom_extensions.append("smc");
        rom_extensions.append("sfc");
        rom_extensions.append("fig");
        rom_extensions.append("1");
        rom_extensions.append("2");

        QString GameName;
        QString Genre(QObject::tr("Unknown"));
        int Year = 0;
        bool bRomFound;

        MythProgressDialog pdial(QObject::tr("Looking for SNES games..."), List->count());
        int progress = 0;

        for (QFileInfoListIterator it(*List); it; ++it)
        {
            pdial.setProgress(progress);
            progress++;

            bRomFound = false;
            QFileInfo Info(*it.current());
            if (IsSnesRom(Info.filePath(),&RHeader, false))
            {
                bRomFound = true;
                char NameBuffer[22];
                for(int i = 0;i < 21;i++)
                {
                    if(RHeader.RomTitle[i] < 32 || RHeader.RomTitle[i] > 126)
                    {
                        NameBuffer[i] = '\0';
                        break;
                    }
                    NameBuffer[i] = RHeader.RomTitle[i];
                }
                NameBuffer[21] = '\0';
                GameName = NameBuffer;
            }
            //The file couldn't be verified.  Check the extension.
            else
            {
                for (QStringList::Iterator i = rom_extensions.begin(); i != rom_extensions.end(); i++)
                {
                    if(Info.extension(false).lower() == *i)
                    {
                        GameName = Info.baseName() + QObject::tr("   (bad checksum)");
                        bRomFound = true;
                        break;
                    }
                }
            }

            cout << GameName << endl;

            if(bRomFound)
            {
                // Put the game into the database.
                thequery = QString("INSERT INTO gamemetadata "
                                   "(system, romname, gamename, genre, year) "
                                   "VALUES (\"Snes\", \"%1\", \"%2\", \"%3\", %4);")
                                   .arg(Info.fileName().latin1())
                                   .arg(GameName.latin1()).arg(Genre.latin1())
                                   .arg(Year);
                query.exec(thequery);
            }
        }

        pdial.Close();
    }
}    

void SnesHandler::processGames()
{
    QString thequery;

    MSqlQuery query(MSqlQuery::InitCon());

    // Remove all metadata entries from the tables, all correct values will be
    // added as they are found.  This is done so that entries that may no longer be
    // available or valid are removed each time the game list is remade.
    query.exec("DELETE FROM gamemetadata WHERE system = \"Snes\";");

    // Search the rom dir for valid new roms.
    QDir RomDir(gContext->GetSetting("SnesRomLocation"));
    const QFileInfoList* List = RomDir.entryInfoList();

    if (!List)
        return;

    RomHeader RHeader;

    MythProgressDialog pdial(QObject::tr("Looking for SNES games..."), List->count());
    int progress = 0;

    for (QFileInfoListIterator it(*List); it; ++it)
    {
        pdial.setProgress(progress);
        progress++;

        QFileInfo Info(*it.current());
        if (IsSnesRom(Info.filePath(),&RHeader, true))
        {
            char NameBuffer[22];
            for(int i = 0;i < 21;i++)
            {
                if(RHeader.RomTitle[i] < 32 || RHeader.RomTitle[i] > 126)
                {
                    NameBuffer[i] = '\0';
                    break;
                }
                NameBuffer[i] = RHeader.RomTitle[i];
            }
            NameBuffer[21] = '\0';
            QString GameName(NameBuffer);
            QString Genre(QObject::tr("Unknown"));
            int Year = 0;

            cout << GameName << endl;

            // Put the game into the database.
            thequery = QString("INSERT INTO gamemetadata "
                               "(system, romname, gamename, genre, year) "
                               "VALUES (\"Snes\", \"%1\", \"%2\", \"%3\", %4);")
                               .arg(Info.fileName().latin1())
                               .arg(GameName.latin1()).arg(Genre.latin1())
                               .arg(Year);
            query.exec(thequery);
        }
    }

    pdial.Close();
}

void SnesHandler::start_game(RomInfo * romdata)
{
    SnesGameSettings game_settings;
    SnesRomInfo *snesdata = dynamic_cast<SnesRomInfo*>(romdata);
    SetGameSettings(game_settings, snesdata);
    QString exec = gContext->GetSetting("SnesBinary") + " ";
    QString emulator = gContext->GetSetting("SnesEmulator", "SNES9x");

    //Set these bools to make seting up the options easier
    bool zsnes = (emulator == "zSNES");
    bool snes9x = !zsnes;

    //bool bNoSound = false;
    if(snes9x)
    {
        if(game_settings.transparency)
            exec+= "-tr ";
        if(game_settings.sixteen)
            exec+= "-16 ";
        if(game_settings.hi_res)
            exec+= "-hi ";
        if(game_settings.no_mode_switch)
            exec+= "-nms ";
        if(game_settings.stretch)
            exec+= "-sc ";
    }
    if(game_settings.full_screen)
        exec+= (snes9x)? "-fs " : "-cs ";
    if(game_settings.no_sound)
        exec+= (snes9x)? "-ns " : "-dd ";
    else
    {
        if(game_settings.stereo)
            exec+= (snes9x)? "-st " : "-z ";
        else
            if(snes9x)
                exec+= " -mono ";
        if(snes9x)
        {
            if(game_settings.envx)
                exec+= "-ex ";
            if(game_settings.thread_sound)
                exec+= "-ts ";
            if(game_settings.sound_sync)
                exec+= "-sy ";
            if(game_settings.interpolated_sound)
                exec+= "-is ";
            if(game_settings.no_sample_caching)
                exec+= "-nc ";
            if(game_settings.alt_sample_decode)
                exec+= "-alt ";
            if(game_settings.no_echo)
                exec+= "-ne ";
            if(game_settings.no_master_volume)
                exec+= "-nmv ";
        }
    }
    if(snes9x)
    {
        if(game_settings.no_joy)
            exec+= "-j ";
        if(game_settings.interleaved)
            exec+= "-i ";
        if(game_settings.alt_interleaved)
            exec+= "-i2 ";
        if(game_settings.header)
            exec+= "-hd ";
        if(game_settings.no_header)
            exec+= "-nhd ";
        if(game_settings.layering)
            exec+= "-l ";
        if(game_settings.no_hdma)
            exec+= "-nh ";
        if(game_settings.no_windows)
            exec+= "-nw ";
        switch(game_settings.interpolate)
        {
            case 1:
                exec+= "-y ";
                break;
            case 2:
                exec+= "-y2 ";
                break;
            case 3:
                exec+= "-y3 ";
                break;
            case 4:
                exec+= "-y4 ";
                break;
            case 5:
                exec+= "-y5";
                break;
            default:
                break;
        }
        if(game_settings.buffer_size != 0)
        {
            exec+= "-bs ";
            exec+= QString::number(game_settings.buffer_size);
            exec+= " ";
        }
        if(game_settings.sound_skip != 0)
        {
            exec+= "-sk ";
            exec+= QString::number(game_settings.sound_skip);
            exec+= " ";
        }
    }
    if(game_settings.hi_rom)
        exec+= (snes9x)? "-hr " : "-h ";
    if(game_settings.low_rom)
        exec+= (snes9x)? "-lr " : "-l ";
    if(game_settings.pal)
        exec+= (snes9x)? "-p " : "-u ";
    if(game_settings.ntsc)
        exec+= (snes9x)? "-ntsc " : "-t ";
    if(game_settings.no_speed_hacks)
        exec+= (snes9x)? "-nospeedhacks " : "-7 ";
    if(game_settings.sound_quality != 0)
    {
        exec+= (snes9x)? "-soundquality " : "-r ";
        exec+= QString::number(game_settings.sound_quality);
        exec+= " ";
    }

    //Disable the menu and mouse if its zsnes
    if(zsnes)
        exec+= "-m -j ";

    exec+= game_settings.extra_options;
    exec+= " \"" + gContext->GetSetting("SnesRomLocation") + "/" + romdata->Romname() + "\"";
    cout << exec << endl;

    // Run the emulator and wait for it to terminate.
    FILE* command = popen(exec, "w");
    pclose(command);
}

void SnesHandler::edit_settings(RomInfo * romdata)
{
    SnesGameSettings game_settings;
    SnesRomInfo *snesdata = dynamic_cast<SnesRomInfo*>(romdata);

    SnesSettingsDlg settingsdlg(snesdata->Romname().latin1());
    settingsdlg.exec();

/*
    QString ImageFile;
    if(snesdata->FindImage("screenshot", &ImageFile))
        settingsdlg.SetScreenshot(ImageFile);
*/
}

void SnesHandler::edit_system_settings(RomInfo * romdata)
{
    romdata = romdata;
    SnesSettingsDlg settingsDlg("default");
    settingsDlg.exec();

    SetDefaultSettings();
}

SnesHandler* SnesHandler::getHandler(void)
{
    if(!pInstance)
    {
        pInstance = new SnesHandler();
    }
    return pInstance;
}

RomInfo* SnesHandler::create_rominfo(RomInfo *parent)
{
    return new SnesRomInfo(*parent);
}

bool SnesHandler::VerifyZipRomHeader(unzFile zf, unsigned int offset, unsigned int &bytesRead, RomHeader* Header)
{
    char buffer[4];
    for(;bytesRead < offset;bytesRead+=4)
    {
        if(unzReadCurrentFile(zf,buffer,4) != 4)
            break;
    }
    if(unzReadCurrentFile(zf,Header,sizeof(RomHeader)) != sizeof(RomHeader))
        return false;
    bytesRead+= sizeof(RomHeader);
    return ((Header->InverseRomChecksum + Header->RomChecksum) == 0xFFFF);
}

bool SnesHandler::VerifyRomHeader(FILE* pFile,unsigned int offset, RomHeader* Header)
{
    fseek(pFile,offset,SEEK_SET);
    if(fread(Header,sizeof(RomHeader),1,pFile) != 1)
        return false;
    return ((Header->InverseRomChecksum + Header->RomChecksum) == 0xFFFF);    
}

void SnesHandler::SetGameSettings(SnesGameSettings &game_settings, SnesRomInfo *rominfo)
{
    game_settings = defaultSettings;
    if(rominfo)
    {
        MSqlQuery query(MSqlQuery::InitCon());
        QString thequery;
        thequery = QString("SELECT * FROM snessettings WHERE romname = \"%1\";")
                          .arg(rominfo->Romname().latin1());
        query.exec(thequery);
        if (query.isActive() && query.size() > 0)
        {
            query.next();
            if (!query.value(1).toBool())
            {
                game_settings.default_options = false;
                game_settings.transparency = query.value(2).toBool();
                game_settings.sixteen = query.value(3).toBool();
                game_settings.hi_res = query.value(4).toBool();
                game_settings.interpolate = query.value(5).toInt();
                game_settings.no_mode_switch = query.value(6).toBool();
                game_settings.full_screen = query.value(7).toBool();
                game_settings.stretch = query.value(8).toBool();
                game_settings.no_sound = query.value(9).toBool();
                game_settings.sound_skip = query.value(10).toInt();
                game_settings.stereo = query.value(11).toBool();
                game_settings.sound_quality = query.value(12).toInt();
                game_settings.envx = query.value(13).toBool();
                game_settings.thread_sound = query.value(14).toBool();
                game_settings.sound_sync = query.value(15).toBool();
                game_settings.interpolated_sound = query.value(16).toBool();
                game_settings.buffer_size = query.value(17).toInt();
                game_settings.no_sample_caching = query.value(18).toBool();
                game_settings.alt_sample_decode = query.value(19).toBool();
                game_settings.no_echo = query.value(20).toBool();
                game_settings.no_master_volume = query.value(21).toBool();
                game_settings.no_joy = query.value(22).toBool();
                game_settings.interleaved = query.value(23).toBool();
                game_settings.alt_interleaved = query.value(24).toBool();
                game_settings.hi_rom = query.value(25).toBool();
                game_settings.low_rom = query.value(26).toBool();
                game_settings.header = query.value(27).toBool();
                game_settings.no_header = query.value(28).toBool();
                game_settings.pal = query.value(29).toBool();
                game_settings.ntsc = query.value(30).toBool();
                game_settings.layering = query.value(31).toBool();
                game_settings.no_hdma = query.value(32).toBool();
                game_settings.no_speed_hacks = query.value(33).toBool();
                game_settings.no_windows = query.value(34).toBool();
                game_settings.extra_options = query.value(35).toString();
            }
        }
    }
}

void SnesHandler::SetDefaultSettings()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT * FROM snessettings WHERE romname = \"default\";");

    if (query.isActive() && query.size() > 0)
    {
        query.next();
        defaultSettings.default_options = query.value(1).toBool();
        defaultSettings.transparency = query.value(2).toBool();
        defaultSettings.sixteen = query.value(3).toBool();
        defaultSettings.hi_res = query.value(4).toBool();
        defaultSettings.interpolate = query.value(5).toInt();
        defaultSettings.no_mode_switch = query.value(6).toBool();
        defaultSettings.full_screen = query.value(7).toBool();
        defaultSettings.stretch = query.value(8).toBool();
        defaultSettings.no_sound = query.value(9).toBool();
        defaultSettings.sound_skip = query.value(10).toInt();
        defaultSettings.stereo = query.value(11).toBool();
        defaultSettings.sound_quality = query.value(12).toInt();
        defaultSettings.envx = query.value(13).toBool();
        defaultSettings.thread_sound = query.value(14).toBool();
        defaultSettings.sound_sync = query.value(15).toBool();
        defaultSettings.interpolated_sound = query.value(16).toBool();
        defaultSettings.buffer_size = query.value(17).toInt();
        defaultSettings.no_sample_caching = query.value(18).toBool();
        defaultSettings.alt_sample_decode = query.value(19).toBool();
        defaultSettings.no_echo = query.value(20).toBool();
        defaultSettings.no_master_volume = query.value(21).toBool();
        defaultSettings.no_joy = query.value(22).toBool();
        defaultSettings.interleaved = query.value(23).toBool();
        defaultSettings.alt_interleaved = query.value(24).toBool();
        defaultSettings.hi_rom = query.value(25).toBool();
        defaultSettings.low_rom = query.value(26).toBool();
        defaultSettings.header = query.value(27).toBool();
        defaultSettings.no_header = query.value(28).toBool();
        defaultSettings.pal = query.value(29).toBool();
        defaultSettings.ntsc = query.value(30).toBool();
        defaultSettings.layering = query.value(31).toBool();
        defaultSettings.no_hdma = query.value(32).toBool();
        defaultSettings.no_speed_hacks = query.value(33).toBool();
        defaultSettings.no_windows = query.value(34).toBool();
        defaultSettings.extra_options = query.value(35).toString();
    }
}

SnesHandler* SnesHandler::pInstance = 0;

