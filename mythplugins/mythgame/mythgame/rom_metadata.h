#ifndef ROMMETADATA_H_
#define ROMMETADATA_H_

// C++
#include <utility>

// Qt
#include <QMap>
#include <QString>

class RomData
{
  public:
    explicit RomData(QString lgenre = "", QString lyear = "",
            QString lcountry = "", QString lgamename = "",
            QString ldescription = "", QString lpublisher = "",
            QString lplatform = "", QString lversion = ""  )
        :       m_genre(std::move(lgenre)),
                m_year(std::move(lyear)),
                m_country(std::move(lcountry)),
                m_gamename(std::move(lgamename)),
                m_description(std::move(ldescription)),
                m_publisher(std::move(lpublisher)),
                m_platform(std::move(lplatform)),
                m_version(std::move(lversion))
            {
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
