#ifndef ROMINFO_H_
#define ROMINFO_H_

#include <qstring.h>


class RomInfo
{
  public:
    RomInfo(QString lromname = "", QString lsystem = "", QString lgamename ="",
            QString lgenre = "", int lyear = 0, bool lfavorite = FALSE)
            {
                romname = lromname;
                system = lsystem;
                gamename = lgamename;
                genre = lgenre;
                year = lyear;
                favorite = lfavorite;
            }
    RomInfo(const RomInfo &lhs)
            {
                romname = lhs.romname;
                system = lhs.system;
                gamename = lhs.gamename;
                genre = lhs.genre;
                year = lhs.year;
                favorite = lhs.favorite;
            }
    virtual ~RomInfo() {}

    QString Romname() const { return romname; }
    void setRomname(const QString &lromname) { romname = lromname; }

    QString System() { return system; }
    void setSystem(const QString &lsystem) { system = lsystem; }

    QString Gamename() { return gamename; }
    void setGamename(const QString &lgamename) { gamename = lgamename; }

    QString Genre() { return genre; }
    void setGenre(const QString &lgenre) { genre = lgenre; }

    int Year() { return year; }
    void setYear(int lyear) { year = lyear; }

    int Favorite() { return favorite; }
    virtual void setFavorite();

    virtual void setField(QString field, QString data);
    virtual void fillData();

    virtual bool FindImage(QString type, QString *result) { type = type;
                                                            result = result;
                                                            return false; }

  protected:
    QString romname;
    QString system;
    QString gamename;
    QString genre;
    int year;
    bool favorite;
};

bool operator==(const RomInfo& a, const RomInfo& b);

#endif
