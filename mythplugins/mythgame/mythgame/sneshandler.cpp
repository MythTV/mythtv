#include <fstream>
#include <iostream>
#include "sneshandler.h"
#include "snessettingsdlg.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qdir.h>

#include <sys/types.h>
#include <unistd.h>

#include <mythtv/mythcontext.h>
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
        QSqlDatabase* db = QSqlDatabase::database();
        
        thequery = "DELETE FROM gamemetadata WHERE system = \"Snes\";";
        db->exec(thequery);

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
        QString Genre("Unknown");
        int Year = 0;
        bool bRomFound;

        MythProgressDialog pdial("Looking for SNES gams...", List->count());
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
                        GameName = Info.baseName() + "   (bad checksum)";
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
                db->exec(thequery);
            }
        }

        pdial.Close();
    }
}    

void SnesHandler::processGames()
{
    QString thequery;

    QSqlDatabase* db = QSqlDatabase::database();

    // Remove all metadata entries from the tables, all correct values will be
    // added as they are found.  This is done so that entries that may no longer be
    // available or valid are removed each time the game list is remade.
    thequery = "DELETE FROM gamemetadata WHERE system = \"Snes\";";
    db->exec(thequery);

    // Search the rom dir for valid new roms.
    QDir RomDir(gContext->GetSetting("SnesRomLocation"));
    const QFileInfoList* List = RomDir.entryInfoList();

    if (!List)
        return;

    RomHeader RHeader;

    MythProgressDialog pdial("Looking for SNES games...", List->count());
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
            QString Genre("Unknown");
            int Year = 0;

            cout << GameName << endl;

            // Put the game into the database.
            thequery = QString("INSERT INTO gamemetadata "
                               "(system, romname, gamename, genre, year) "
                               "VALUES (\"Snes\", \"%1\", \"%2\", \"%3\", %4);")
                               .arg(Info.fileName().latin1())
                               .arg(GameName.latin1()).arg(Genre.latin1())
                               .arg(Year);
            db->exec(thequery);
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
    //bool bNoSound = false;
    if(game_settings.transparency)
        exec+= "-tr ";
    if(game_settings.sixteen)
        exec+= "-16 ";
    if(game_settings.hi_res)
        exec+= "-hi ";
    if(game_settings.no_mode_switch)
        exec+= "-nms ";
    if(game_settings.full_screen)
        exec+= "-fs ";
    if(game_settings.stretch)
        exec+= "-sc ";
    if(game_settings.no_sound)
        exec+= "-ns ";
    else
    {
        if(game_settings.stereo)
            exec+= "-st ";
        else
            exec+= " -mono ";
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
    if(game_settings.no_joy)
        exec+= "-j ";
    if(game_settings.interleaved)
        exec+= "-i ";
    if(game_settings.alt_interleaved)
        exec+= "-i2 ";
    if(game_settings.hi_rom)
        exec+= "-hr ";
    if(game_settings.low_rom)
        exec+= "-lr ";
    if(game_settings.header)
        exec+= "-hd ";
    if(game_settings.no_header)
        exec+= "-nhd ";
    if(game_settings.pal)
        exec+= "-p ";
    if(game_settings.ntsc)
        exec+= "-ntsc ";
    if(game_settings.layering)
        exec+= "-l ";
    if(game_settings.no_hdma)
        exec+= "-nh ";
    if(game_settings.no_speed_hacks)
        exec+= "-nospeedhacks ";
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
    if(game_settings.sound_skip != 0)
    {
        exec+= "-sk ";
        exec+= QString::number(game_settings.sound_skip);
        exec+= " ";
    }
    if(game_settings.sound_quality != 0)
    {
        exec+= "-soundquality ";
        exec+= QString::number(game_settings.sound_quality);
        exec+= " ";
    }
    if(game_settings.buffer_size != 0)
    {
        exec+= "-bs ";
        exec+= QString::number(game_settings.buffer_size);
        exec+= " ";
    }
    exec+= game_settings.extra_options;
    exec+= " \"" + gContext->GetSetting("SnesRomLocation") + "/" + romdata->Romname() + "\"";
    cout << exec << endl;

    // Run the emulator and wait for it to terminate.
    FILE* command = popen(exec, "w");
    pclose(command);
}

void SnesHandler::edit_settings(QWidget *parent,RomInfo * romdata)
{
    SnesGameSettings game_settings;
    SnesRomInfo *snesdata = dynamic_cast<SnesRomInfo*>(romdata);
    SetGameSettings(game_settings, snesdata);

    SnesSettingsDlg settingsdlg(parent, "gamesettings", true);
    QString ImageFile;
    if(snesdata->FindImage("screenshot", &ImageFile))
        settingsdlg.SetScreenshot(ImageFile);
    if(settingsdlg.Show(&game_settings))
        SaveGameSettings(game_settings, snesdata);
}

void SnesHandler::edit_system_settings(QWidget *parent,RomInfo * romdata)
{
    romdata = romdata;
    SnesSettingsDlg settingsDlg(parent, "snessettings", true, true);
    if(settingsDlg.Show(&defaultSettings))
        SaveGameSettings(defaultSettings, NULL);    
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
        QSqlDatabase *db = QSqlDatabase::database();
        QString thequery;
        thequery = QString("SELECT * FROM snessettings WHERE romname = \"%1\";")
                          .arg(rominfo->Romname().latin1());
        QSqlQuery query = db->exec(thequery);
        if (query.isActive() && query.numRowsAffected() > 0)
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
    QSqlDatabase *db = QSqlDatabase::database();
    QSqlQuery query = db->exec("SELECT * FROM snessettings WHERE romname = \"default\";");

    if (query.isActive() && query.numRowsAffected() > 0)
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

void SnesHandler::SaveGameSettings(SnesGameSettings &game_settings, SnesRomInfo *rominfo)
{
    QSqlDatabase *db = QSqlDatabase::database();
    QString thequery;
    QString RomName = "default";
    if(rominfo)
        RomName = rominfo->Romname();
    thequery = QString("SELECT romname FROM snessettings WHERE romname = \"%1\";").arg(RomName.latin1());
    QSqlQuery query = db->exec(thequery);
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        thequery.sprintf("UPDATE snessettings SET usedefault = %d, transparency = %d, "
                          "sixteen = %d, hires = %d, interpolate = %d, "
                          "nomodeswitch = %d, fullscreen = %d, stretch = %d, nosound = %d, "
                          "soundskip = %d, stereo = %d, soundquality = %d, envx = %d, "
                          "threadsound = %d, syncsound = %d, interpolatedsound = %d, buffersize = %d, "
                          "nosamplecaching = %d, altsampledecode = %d, noecho = %d , "
                          "nomastervolume = %d, nojoy = %d, interleaved = %d, "
                          "altinterleaved = %d, hirom = %d, lowrom = %d, header = %d, "
                          "noheader = %d, pal = %d, ntsc = %d, layering = %d, nohdma = %d, nospeedhacks = %d, "
                          "nowindows = %d, extraoption = \"%s\" WHERE romname = \"%s\";",
                          game_settings.default_options, game_settings.transparency,
                          game_settings.sixteen, game_settings.hi_res,
                          game_settings.interpolate, game_settings.no_mode_switch,
                          game_settings.full_screen, game_settings.stretch,
                          game_settings.no_sound, game_settings.sound_skip, game_settings.stereo,
                          game_settings.sound_quality, game_settings.envx,
                          game_settings.thread_sound, game_settings.sound_sync, game_settings.interpolated_sound,
                          game_settings.buffer_size, game_settings.no_sample_caching,
                          game_settings.alt_sample_decode, game_settings.no_echo,
                          game_settings.no_master_volume, game_settings.no_joy, game_settings.interleaved,
                          game_settings.alt_interleaved, game_settings.hi_rom,
                          game_settings.low_rom, game_settings.header, game_settings.no_header,
                          game_settings.pal, game_settings.ntsc, game_settings.layering,
                          game_settings.no_hdma, game_settings.no_speed_hacks,
                          game_settings.no_windows, game_settings.extra_options.latin1(),
                          RomName.latin1());
    }
    else
    {
        thequery.sprintf("INSERT INTO snessettings (romname,usedefault,transparency,sixteen,"
                          "hires,interpolate,nomodeswitch,fullscreen,stretch,"
                          "nosound,soundskip,stereo,soundquality,envx,threadsound,syncsound,"
                          "interpolatedsound,buffersize,nosamplecaching,altsampledecode,noecho,nomastervolume,"
                          "nojoy,interleaved,altinterleaved,hirom,lowrom,header,noheader,"
                          "pal,ntsc,layering,nohdma,nospeedhacks,nowindows,extraoption) VALUES "
                          "(\"%s\",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                          "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\"%s\");",RomName.latin1(),
                          game_settings.default_options, game_settings.transparency,
                          game_settings.sixteen, game_settings.hi_res,
                          game_settings.interpolate, game_settings.no_mode_switch,
                          game_settings.full_screen, game_settings.stretch,
                          game_settings.no_sound, game_settings.sound_skip, game_settings.stereo,
                          game_settings.sound_quality, game_settings.envx,
                          game_settings.thread_sound, game_settings.sound_sync, game_settings.interpolated_sound,
                          game_settings.buffer_size, game_settings.no_sample_caching,
                          game_settings.alt_sample_decode, game_settings.no_echo,
                          game_settings.no_master_volume, game_settings.no_joy, game_settings.interleaved,
                          game_settings.alt_interleaved, game_settings.hi_rom,
                          game_settings.low_rom, game_settings.header, game_settings.no_header,
                          game_settings.pal, game_settings.ntsc, game_settings.layering,
                          game_settings.no_hdma, game_settings.no_speed_hacks,
                          game_settings.no_windows, game_settings.extra_options.latin1());
    }
    query = db->exec(thequery);
} 

SnesHandler* SnesHandler::pInstance = 0;

