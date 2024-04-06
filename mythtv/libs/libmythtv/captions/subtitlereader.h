#ifndef SUBTITLEREADER_H
#define SUBTITLEREADER_H

#include <QMutex>

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "libmythbase/mythchrono.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdeque.h"
#include "textsubtitleparser.h"

#define DEBUG_SUBTITLES 1
#ifdef DEBUG_SUBTITLES
QString toString(const AVSubtitle& sub);
#endif

class MythPlayer;

class AVSubtitles
{
  public:
    AVSubtitles() = default;
    MythDeque<AVSubtitle> m_buffers;
    QMutex                m_lock;
    bool                  m_fixPosition {false};
    bool                  m_needSync    {false};
};

class RawTextSubs
{
  public:
    RawTextSubs(void) = default;

    QStringList m_buffers;
    std::chrono::milliseconds m_duration {0ms};
    QMutex      m_lock;
};

class SubtitleReader : public QObject
{
    Q_OBJECT

  signals:
    void TextSubtitlesUpdated();

  public:
    SubtitleReader(MythPlayer *parent);
   ~SubtitleReader() override;

    void EnableAVSubtitles(bool enable);
    void EnableTextSubtitles(bool enable);
    void EnableRawTextSubtitles(bool enable);

    AVSubtitles* GetAVSubtitles(void) { return &m_avSubtitles; }
    bool AddAVSubtitle(AVSubtitle& subtitle, bool fix_position,
                       bool is_selected_forced_track, bool allow_forced,
                       bool isExternal);
    void ClearAVSubtitles(void);
    static void FreeAVSubtitle(AVSubtitle &sub);

    TextSubtitleParser* GetParser(void) { return m_externalParser; }
    bool HasTextSubtitles(void);
    void LoadExternalSubtitles(const QString &subtitleFileName, bool isInProgress);
    int ReadNextSubtitle(void);
    void SeekFrame(int64_t ts, int flags);

    QStringList GetRawTextSubtitles(std::chrono::milliseconds &duration);
    void AddRawTextSubtitle(const QStringList& list, std::chrono::milliseconds duration);
    void ClearRawTextSubtitles(void);

  private:
    MythPlayer    *m_parent                 {nullptr};

    AVSubtitles   m_avSubtitles;
    bool          m_avSubtitlesEnabled      {false};
    TextSubtitles m_textSubtitles;
    bool          m_textSubtitlesEnabled    {false};
    RawTextSubs   m_rawTextSubtitles;
    bool          m_rawTextSubtitlesEnabled {false};

    TextSubtitleParser *m_externalParser    {nullptr};
};

#endif // SUBTITLEREADER_H
