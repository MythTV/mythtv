#ifndef ROMINFO_H_
#define ROMINFO_H_

#include <qstring.h>

int romInDB(QString rom, QString gametype);

class RomInfo
{
  public:
    RomInfo(QString lromname = "", QString lsystem = "", QString lgamename ="",
            QString lgenre = "", int lyear = 0, bool lfavorite = FALSE, 
            QString lrompath = "", QString lcountry ="", QString lcrc_value = "",
            int ldiskcount = 0, QString lgametype = "", int lromcount = 0,
            QString lallsystems = "")
            {
                romname = lromname;
                system = lsystem;
                gamename = lgamename;
                genre = lgenre;
                year = lyear;
                favorite = lfavorite;
                rompath = lrompath;
                country = lcountry;
                crc_value = lcrc_value;
                diskcount = ldiskcount;
                gametype = lgametype;
                romcount = lromcount;
                allsystems = lallsystems;
            }

    RomInfo(const RomInfo &lhs)
            {
                romname = lhs.romname;
                system = lhs.system;
                gamename = lhs.gamename;
                genre = lhs.genre;
                year = lhs.year;
                favorite = lhs.favorite;
                rompath = lhs.rompath;
                country = lhs.country;
                crc_value = lhs.crc_value;
                diskcount = lhs.diskcount;
                gametype = lhs.gametype;
                romcount = lhs.romcount;
                allsystems = lhs.allsystems;
            }

    virtual ~RomInfo() {}

    bool FindImage(QString BaseFileName, QString *result);

    QString Rompath() const { return rompath; }
    void setRompath(const QString &lrompath) { rompath = lrompath; }

    QString Romname() const { return romname; }
    void setRomname(const QString &lromname) { romname = lromname; }

    QString System() { return system; }
    void setSystem(const QString &lsystem) { system = lsystem; }

    QString Gamename() { return gamename; }
    void setGamename(const QString &lgamename) { gamename = lgamename; }

    QString Genre() { return genre; }
    void setGenre(const QString &lgenre) { genre = lgenre; }
    
    QString Country() { return country; }
    void setCountry(const QString &lcountry) { country = lcountry; }

    QString GameType() { return gametype; }
    void setGameType(const QString &lgametype) { gametype = lgametype; }

    int RomCount() { return romcount; }
    void setRomCount(const int &lromcount) { romcount = lromcount; }

    QString AllSystems() { return allsystems; }
    void setAllSystems(const QString &lallsystems) { allsystems = lallsystems; }

    int DiskCount() { return diskcount; }
    void setDiskCount(const int &ldiskcount) { diskcount = ldiskcount; }

    QString CRC_VALUE() { return crc_value; }
    void setCRC_VALUE(const QString &lcrc_value) { crc_value = lcrc_value; }

    QString ImagePath() { return imagepath; }
    void setImagePath(const QString &limagepath) { imagepath = limagepath; } 

    int Year() { return year; }
    void setYear(int lyear) { year = lyear; }

    int Favorite() { return favorite; }
     virtual void setFavorite();

    QString getExtension();

    virtual void setField(QString field, QString data);
    virtual void fillData();
    virtual void edit_rominfo();

  protected:
    QString romname;
    QString system;
    QString gamename;
    QString genre;
    QString imagepath; 
    QString country;
    QString crc_value;
    QString gametype;
    QString allsystems;
    int romcount;
    int diskcount;
    int year;
    bool favorite;
    QString rompath;
};

bool operator==(const RomInfo& a, const RomInfo& b);

#endif
