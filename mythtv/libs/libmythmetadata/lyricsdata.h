#ifndef LYRICSDATA_H_
#define LYRICSDATA_H_

// C/C++
#include <cstdint>
#include <utility>

// qt
#include <QObject>

// MythTV
#include "libmythbase/mythdate.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythmetadata/musicmetadata.h"
#include "libmythmetadata/mythmetaexp.h"

class LyricsData;
class TestLyrics;

class META_PUBLIC LyricsLine
{
  public:
    LyricsLine() = default;
    LyricsLine(std::chrono::milliseconds time, QString lyric) :
        m_time(time), m_lyric(std::move(lyric)) { }

    std::chrono::milliseconds m_time {0ms};
    QString m_lyric;

    QString toString(bool syncronized)
    {
        if (syncronized)
            return formatTime() + m_lyric;

        return m_lyric;
    }

  private:
    QString formatTime(void) const
    {
        QString timestr = MythDate::formatTime(m_time,"mm:ss.zz");
        return QString("[%1]").arg(timestr);
    }
};

using LyricsLineMap = QMap<std::chrono::milliseconds, LyricsLine*>;

class META_PUBLIC LyricsData : public QObject
{
  Q_OBJECT

  friend class TestLyrics;

  public:
    LyricsData() = default;
    explicit LyricsData(MusicMetadata *parent)
        : m_parent(parent) {}
    LyricsData(MusicMetadata *parent, QString grabber, QString artist,
               QString album, QString title, bool syncronized)
        : m_parent(parent), m_grabber(std::move(grabber)),
          m_artist(std::move(artist)), m_album(std::move(album)),
          m_title(std::move(title)), m_syncronized(syncronized) {}
    ~LyricsData() override;

    QString grabber(void) { return m_grabber; }
    void setGrabber(const QString &grabber) { m_grabber = grabber; }

    QString artist(void) { return m_artist; }
    void setArtist(const QString &artist) { m_artist = artist; }

    QString album(void) { return m_album; }
    void setAlbum(const QString &album) { m_album = album; }

    QString title(void) { return m_title; }
    void setTitle(const QString &title) { m_title = title; }

    LyricsLineMap* lyrics(void) { return &m_lyricsMap; }
    void setLyrics(const QStringList &lyrics);

    bool syncronized(void) const { return m_syncronized; }
    void setSyncronized(bool syncronized ) { m_syncronized = syncronized; }

    bool changed(void) const { return m_changed; }
    void setChanged(bool changed) { m_changed = changed; }

    enum Status : std::uint8_t
    {
      STATUS_NOTLOADED = 0, // not looked for any lyrics yet
      STATUS_SEARCHING,     // search is taking place
      STATUS_FOUND,         // search completed and some lyrics have been found
      STATUS_NOTFOUND       // search completed but no lyrics have been found
    };

    Status getStatus(void) { return m_status; }

    void clear(void);
    void clearLyrics(void);
    void findLyrics(const QString &grabber);
    void save(void);

    void customEvent(QEvent *event) override; // QObject

  signals:
    void statusChanged(LyricsData::Status status, const QString &message);

  private:
    void loadLyrics(const QString &xmlData);
    QString createLyricsXML(void);

    LyricsLineMap m_lyricsMap;

    MusicMetadata *m_parent {nullptr};

    Status  m_status        {STATUS_NOTLOADED};

    QString m_grabber;
    QString m_artist;
    QString m_album;
    QString m_title;
    bool    m_syncronized   {false};
    bool    m_changed       {false};
};

Q_DECLARE_METATYPE(LyricsLine*)

#endif
