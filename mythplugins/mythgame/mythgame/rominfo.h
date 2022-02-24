#ifndef ROMINFO_H_
#define ROMINFO_H_

// C++
#include <utility>

// Qt
#include <QList>
#include <QMetaType>
#include <QString>

int romInDB(const QString& rom, const QString& gametype);

class RomInfo
{
  public:
    static QList<RomInfo*> GetAllRomInfo();
    static RomInfo *GetRomInfoById(int id);

    explicit RomInfo(int lid = 0, QString lromname = "", QString lsystem = "", QString lgamename ="",
            QString lgenre = "", QString lyear = "", bool lfavorite = false,
            QString lrompath = "", QString lcountry ="", QString lcrc_value = "",
            int ldiskcount = 0, QString lgametype = "", int lromcount = 0,
            QString lallsystems = "", QString lplot = "", QString lpublisher = "",
            QString lversion = "", QString lscreenshot = "", QString lfanart = "",
            QString lboxart = "", QString linetref = "")
        :       m_id(lid),
                m_romname(std::move(lromname)),
                m_system(std::move(lsystem)),
                m_gamename(std::move(lgamename)),
                m_genre(std::move(lgenre)),
                m_country(std::move(lcountry)),
                m_crcValue(std::move(lcrc_value)),
                m_gametype(std::move(lgametype)),
                m_allsystems(std::move(lallsystems)),
                m_plot(std::move(lplot)),
                m_publisher(std::move(lpublisher)),
                m_version(std::move(lversion)),
                m_romcount(lromcount),
                m_diskcount(ldiskcount),
                m_year(std::move(lyear)),
                m_favorite(lfavorite),
                m_rompath(std::move(lrompath)),
                m_screenshot(std::move(lscreenshot)),
                m_fanart(std::move(lfanart)),
                m_boxart(std::move(lboxart)),
                m_inetref(std::move(linetref))
            {
            }

    RomInfo(const RomInfo &lhs) = default;

    ~RomInfo() = default;

    static bool FindImage(QString BaseFileName, QString *result);

    int Id() const { return m_id; }
    void setId(int lid) { m_id = lid; }

    QString Rompath() const { return m_rompath; }
    void setRompath(const QString &lrompath) { m_rompath = lrompath; }

    QString Screenshot() const { return m_screenshot; }
    void setScreenshot(const QString &lscreenshot) { m_screenshot = lscreenshot; }

    QString Fanart() const { return m_fanart; }
    void setFanart(const QString &lfanart) { m_fanart = lfanart; }

    QString Boxart() const { return m_boxart; }
    void setBoxart(const QString &lboxart) { m_boxart = lboxart; }

    QString Romname() const { return m_romname; }
    void setRomname(const QString &lromname) { m_romname = lromname; }

    QString System() const { return m_system; }
    void setSystem(const QString &lsystem) { m_system = lsystem; }

    QString Gamename() const { return m_gamename; }
    void setGamename(const QString &lgamename) { m_gamename = lgamename; }

    QString Genre() const { return m_genre; }
    void setGenre(const QString &lgenre) { m_genre = lgenre; }

    QString Country() const { return m_country; }
    void setCountry(const QString &lcountry) { m_country = lcountry; }

    QString GameType() const { return m_gametype; }
    void setGameType(const QString &lgametype) { m_gametype = lgametype; }

    int RomCount() const { return m_romcount; }
    void setRomCount(int lromcount) { m_romcount = lromcount; }

    QString AllSystems() const { return m_allsystems; }
    void setAllSystems(const QString &lallsystems) { m_allsystems = lallsystems; }

    int DiskCount() const { return m_diskcount; }
    void setDiskCount(int ldiskcount) { m_diskcount = ldiskcount; }

    QString CRC_VALUE() const { return m_crcValue; }
    void setCRC_VALUE(const QString &lcrc_value) { m_crcValue = lcrc_value; }

    QString Plot() const { return m_plot; }
    void setPlot(const QString &lplot) { m_plot = lplot; }

    QString Publisher() const { return m_publisher; }
    void setPublisher(const QString &lpublisher) { m_publisher = lpublisher; }

    QString Version() const { return m_version; }
    void setVersion(const QString &lversion) { m_version = lversion; }

    QString Year() const { return m_year; }
    void setYear(const QString &lyear) { m_year = lyear; }

    QString Inetref() const { return m_inetref; }
    void setInetref(const QString &linetref) { m_inetref = linetref; }

    bool Favorite() const { return m_favorite; }
    void setFavorite(bool updateDatabase = false);

    QString getExtension() const;
    QString toString() const;

    void setField(const QString& field, const QString& data);
    void fillData();

    void SaveToDatabase() const;
    void DeleteFromDatabase() const;

  protected:
    int     m_id;
    QString m_romname;
    QString m_system;
    QString m_gamename;
    QString m_genre;
    QString m_country;
    QString m_crcValue;
    QString m_gametype;
    QString m_allsystems;
    QString m_plot;
    QString m_publisher;
    QString m_version;
    int     m_romcount;
    int     m_diskcount;
    QString m_year;
    bool    m_favorite;
    QString m_rompath;
    QString m_screenshot;
    QString m_fanart;
    QString m_boxart;
    QString m_inetref;
};

bool operator==(const RomInfo& a, const RomInfo& b);

Q_DECLARE_METATYPE(RomInfo *)

#endif
