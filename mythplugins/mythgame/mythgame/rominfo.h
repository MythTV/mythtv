#ifndef ROMINFO_H_
#define ROMINFO_H_

#include <qstring.h>

class QSqlDatabase;

class RomInfo
{
  public:
    RomInfo(QString lromname = "", QString lsystem = "", QString lgamename ="",
            QString lgenre = "", int lyear = 0)
            {
                romname = lromname;
                system = lsystem;
                gamename = lgamename;
                genre = lgenre;
                year = lyear;
            }
    RomInfo(const RomInfo &lhs)
            {
                romname = lhs.romname;
                system = lhs.system;
                gamename = lhs.gamename;
                genre = lhs.genre;
                year = lhs.year;
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

    virtual void setField(QString field, QString data);
    virtual void fillData(QSqlDatabase *db);

    virtual bool FindImage(QString type, QString *result) { return false; }

  protected:
    QString romname;
    QString system;
    QString gamename;
    QString genre;
    int year;
};

bool operator==(const RomInfo& a, const RomInfo& b);

#endif
