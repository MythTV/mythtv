#ifndef ROMMETADATA_H_
#define ROMMETADATA_H_

#include <QString>
#include <QMap>

class RomData
{
  public:
    RomData(QString lgenre = "", QString lyear = "",
            QString lcountry = "", QString lgamename = "",
            QString ldescription = "", QString lpublisher = "",
            QString lplatform = "", QString lversion = ""  )
            {
                genre = lgenre;
                year = lyear;
                country = lcountry;
                gamename = lgamename;
                description = ldescription;
                publisher = lpublisher;
                platform = lplatform;
                version = lversion;
            }

    QString Genre() const { return genre; }
    QString Year() const { return year; }
    QString Country() const { return country; }
    QString GameName() const { return gamename; }
    QString Description() const { return description; }
    QString Publisher() const { return publisher; }
    QString Platform() const { return platform; }
    QString Version() const { return version; }

  private:
    QString genre;
    QString year;
    QString country;
    QString gamename;
    QString description;
    QString publisher;
    QString platform;
    QString version;
};

typedef QMap <QString, RomData> RomDBMap;

QString crcStr(int crc);

QString crcinfo(QString romname, QString GameType, QString *key, RomDBMap *romDB);

#endif
