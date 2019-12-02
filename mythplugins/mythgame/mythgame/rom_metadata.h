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
                m_genre = lgenre;
                m_year = lyear;
                m_country = lcountry;
                m_gamename = lgamename;
                m_description = ldescription;
                m_publisher = lpublisher;
                m_platform = lplatform;
                m_version = lversion;
            }

    QString Genre() const { return m_genre; }
    QString Year() const { return m_year; }
    QString Country() const { return m_country; }
    QString GameName() const { return m_gamename; }
    QString Description() const { return m_description; }
    QString Publisher() const { return m_publisher; }
    QString Platform() const { return m_platform; }
    QString Version() const { return m_version; }

  private:
    QString m_genre;
    QString m_year;
    QString m_country;
    QString m_gamename;
    QString m_description;
    QString m_publisher;
    QString m_platform;
    QString m_version;
};

using RomDBMap = QMap <QString, RomData>;

QString crcStr(int crc);

QString crcinfo(const QString& romname, const QString& GameType, QString *key, RomDBMap *romDB);

#endif
