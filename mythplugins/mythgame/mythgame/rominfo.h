#ifndef ROMINFO_H_
#define ROMINFO_H_

#include <QMetaType>
#include <QString>

int romInDB(QString rom, QString gametype);

class RomInfo
{
  public:
    RomInfo(QString lromname = "", QString lsystem = "", QString lgamename ="",
            QString lgenre = "", QString lyear = "", bool lfavorite = FALSE,
            QString lrompath = "", QString lcountry ="", QString lcrc_value = "",
            int ldiskcount = 0, QString lgametype = "", int lromcount = 0,
            QString lallsystems = "", QString lplot = "", QString lpublisher = "",
            QString lversion = "", QString lscreenshot = "", QString lfanart = "",
            QString lboxart = "", QString linetref = "")
            {
                romname = lromname;
                system = lsystem;
                gamename = lgamename;
                genre = lgenre;
                year = lyear;
                favorite = lfavorite;
                rompath = lrompath;
                screenshot = lscreenshot;
                fanart = lfanart;
                boxart = lboxart;
                country = lcountry;
                crc_value = lcrc_value;
                diskcount = ldiskcount;
                gametype = lgametype;
                romcount = lromcount;
                allsystems = lallsystems;
                plot = lplot;
                publisher = lpublisher;
                version = lversion;
                inetref = linetref;
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
                screenshot = lhs.screenshot;
                fanart = lhs.fanart;
                boxart = lhs.boxart;
                country = lhs.country;
                crc_value = lhs.crc_value;
                diskcount = lhs.diskcount;
                gametype = lhs.gametype;
                romcount = lhs.romcount;
                allsystems = lhs.allsystems;
                plot = lhs.plot;
                publisher = lhs.publisher;
                version = lhs.version;
                inetref = lhs.inetref;
            }

    ~RomInfo() {}

    bool FindImage(QString BaseFileName, QString *result);

    QString Rompath() const { return rompath; }
    void setRompath(const QString &lrompath) { rompath = lrompath; }

    QString Screenshot() const { return screenshot; }
    void setScreenshot(const QString &lscreenshot) { screenshot = lscreenshot; }

    QString Fanart() const { return fanart; }
    void setFanart(const QString &lfanart) { fanart = lfanart; }

    QString Boxart() const { return boxart; }
    void setBoxart(const QString &lboxart) { boxart = lboxart; }

    QString Romname() const { return romname; }
    void setRomname(const QString &lromname) { romname = lromname; }

    QString System() const { return system; }
    void setSystem(const QString &lsystem) { system = lsystem; }

    QString Gamename() const { return gamename; }
    void setGamename(const QString &lgamename) { gamename = lgamename; }

    QString Genre() const { return genre; }
    void setGenre(const QString &lgenre) { genre = lgenre; }

    QString Country() const { return country; }
    void setCountry(const QString &lcountry) { country = lcountry; }

    QString GameType() const { return gametype; }
    void setGameType(const QString &lgametype) { gametype = lgametype; }

    int RomCount() const { return romcount; }
    void setRomCount(const int &lromcount) { romcount = lromcount; }

    QString AllSystems() const { return allsystems; }
    void setAllSystems(const QString &lallsystems) { allsystems = lallsystems; }

    int DiskCount() const { return diskcount; }
    void setDiskCount(const int &ldiskcount) { diskcount = ldiskcount; }

    QString CRC_VALUE() const { return crc_value; }
    void setCRC_VALUE(const QString &lcrc_value) { crc_value = lcrc_value; }

    QString Plot() const { return plot; }
    void setPlot(const QString &lplot) { plot = lplot; }

    QString Publisher() const { return publisher; }
    void setPublisher(const QString &lpublisher) { publisher = lpublisher; }

    QString Version() const { return version; }
    void setVersion(const QString &lversion) { version = lversion; }

    QString Year() const { return year; }
    void setYear(const QString &lyear) { year = lyear; }

    QString Inetref() const { return inetref; }
    void setInetref(const QString &linetref) { inetref = linetref; }

    int Favorite() const { return favorite; }
    void setFavorite(bool updateDatabase = false);

    QString getExtension();

    void setField(QString field, QString data);
    void fillData();
    void UpdateDatabase();

  protected:
    QString romname;
    QString system;
    QString gamename;
    QString genre;
    QString country;
    QString crc_value;
    QString gametype;
    QString allsystems;
    QString plot;
    QString publisher;
    QString version;
    int romcount;
    int diskcount;
    QString year;
    bool favorite;
    QString rompath;
    QString screenshot;
    QString fanart;
    QString boxart;
    QString inetref;
};

bool operator==(const RomInfo& a, const RomInfo& b);

Q_DECLARE_METATYPE(RomInfo *)

#endif
