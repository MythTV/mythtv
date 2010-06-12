#ifndef SUBTITLEREADER_H
#define SUBTITLEREADER_H

#include <QMutex>

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "mythdeque.h"
#include "textsubtitleparser.h"

class AVSubtitles
{
  public:
    MythDeque<AVSubtitle> buffers;
    QMutex lock;
};

class SubtitleReader
{
  public:
    SubtitleReader();
    ~SubtitleReader();

    void EnableAVSubtitles(bool enable);
    void EnableTextSubtitles(bool enable);

    AVSubtitles* GetAVSubtitles(void) { return &m_AVSubtitles; }
    void AddAVSubtitle(const AVSubtitle& subtitle);
    void ClearAVSubtitles(void);
    void FreeAVSubtitle(const AVSubtitle &sub);

    TextSubtitles* GetTextSubtitles(void) { return &m_TextSubtitles; }
    bool HasTextSubtitles(void);
    bool LoadExternalSubtitles(const QString &videoFile);

  private:
    AVSubtitles   m_AVSubtitles;
    bool          m_AVSubtitlesEnabled;
    TextSubtitles m_TextSubtitles;
    bool          m_TextSubtitlesEnabled;
};

#endif // SUBTITLEREADER_H
