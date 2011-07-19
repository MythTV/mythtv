// -*- Mode: c++ -*-

#ifndef MYTHCCEXTRACTORPLAYER_H
#define MYTHCCEXTRACTORPLAYER_H

#include <stdint.h>

#include <QStringList>
#include <QImage>
#include <QPoint>
#include <QHash>

#include "mythplayer.h"
#include "teletextextractorreader.h"

/**
 * Represents one subtitle record.
 */

struct OneSubtitle
{
    /// Time we have to start showing subtitle, msec.
    int64_t start_time;
    /// Time we have to show subtitle, msec.
    int64_t length;
    /// Is this a text subtitle.
    bool is_text;
    /// Lines of text of subtitles.
    QStringList text;
    /// Image of subtitle.
    QImage img;
    /// Shift of image on the screen.
    QPoint img_shift;

    OneSubtitle() :
        start_time(),
        length(-1),
        is_text(true),
        text(),
        img_shift(0, 0)
    {}

    bool operator==(const OneSubtitle &x) const
    {
        if (is_text != x.is_text)
            return false;
        if (is_text && text != x.text)
            return false;
        if (!is_text && img != x.img)
            return false;
        if (!is_text && img_shift != x.img_shift)
            return false;
        if (start_time != x.start_time)
            return false;
        if (length != x.length)
            return false;

        return true;
    }
};

class MTV_PUBLIC MythCCExtractorPlayer : public MythPlayer
{
  public:
    /// Key is a page number, values are the subtitles in chrono order.
    typedef QHash<int, QList<OneSubtitle> > TTXStreamType;
    /// Key is a CC number (1-4), values are the subtitles in chrono order.
    typedef QHash<int, QList<OneSubtitle> > CC608StreamType;
    /// Key is a CC service (1-63), values are the subtitles in chrono order.
    typedef QHash<int, QList<OneSubtitle> > CC708StreamType;
    /// Subtitles in chrono order.
    typedef QList<OneSubtitle> DVBStreamType;

    MythCCExtractorPlayer(bool showProgress) :
        MythPlayer(true /*muted*/),
        m_curTime(0),
        m_curTimeShift(-1),
        m_myFramesPlayed(0),
        m_showProgress(showProgress)
    {
    }

    ~MythCCExtractorPlayer();

    bool run(void);

    virtual CC708Reader    *GetCC708Reader(uint id=0);
    virtual CC608Reader    *GetCC608Reader(uint id=0);
    virtual SubtitleReader *GetSubReader(uint id=0);
    virtual TeletextReader *GetTeletextReader(uint id=0);

    QList<uint> GetCC708StreamsList(void) const
    {
        return cc708.keys();
    }

    QList<uint> GetCC608StreamsList(void) const
    {
        return cc608.keys();
    }

    QList<uint> GetSubtitleStreamsList(void) const
    {
        return subReader.keys();
    }

    QList<uint> GetTTXStreamsList(void) const
    {
        return ttxReader.keys();
    }

    TTXStreamType GetTTXSubtitles(uint stream_id = 0) const
    {
        return ttxSubtitles.value(stream_id);
    }

    CC608StreamType GetCC608Subtitles(uint stream_id = 0) const
    {
        return cc608Subtitles.value(stream_id);
    }

    CC708StreamType GetCC708Subtitles(uint stream_id = 0) const
    {
        return cc708Subtitles.value(stream_id);
    }

    DVBStreamType GetDVBSubtitles(uint stream_id = 0) const
    {
        return dvbSubtitles.value(stream_id);
    }

  private:
    void ProcessNewSubtitle(QStringList content, QList<OneSubtitle> &list);
    void ProcessNewSubtitle(OneSubtitle content, QList<OneSubtitle> &list);

    void Process608Subtitles(void);
    void GotNew608Subtitle(uint streamId, int ccIdx, CC608Buffer *subtitles);
    void Finish608SubtitlesProcess(void);

    void ProcessTTXSubtitles(void);
    void GotNewTTXPage(uint stream_id, const TeletextSubPage &subPage);
    void FinishTTXSubtitlesProcess(void);

    static QStringList ConvertTTXPage(const TeletextSubPage &subPage);

    void Process708Subtitles(void);
    void GotNew708Subtitle(uint streamId, int serviceIdx, int windowIdx,
                           const std::vector<CC708String *> &content);
    void Finish708SubtitlesProcess(void);

    void ProcessDVBSubtitles(void);
    void FinishDVBSubtitlesProcess(void);

    void OnGotNewFrame(void);
    void OnDecodingFinished(void);

    QHash<uint, CC708Reader*>    cc708;
    QHash<uint, CC608Reader*>    cc608;
    QHash<uint, SubtitleReader*> subReader;
    QHash<uint, TeletextExtractorReader*> ttxReader;

    /// Keeps read subtitles from teletext for each ttx stream.
    QHash<uint, TTXStreamType> ttxSubtitles;
    /// Keeps read cc608 subtitles from for each cc608 stream.
    QHash<uint, CC608StreamType> cc608Subtitles;
    /// Keeps read cc708 subtitles from for each cc608 stream.
    QHash<uint, CC708StreamType> cc708Subtitles;
    /// Keeps read DVB subtitles
    QHash<uint, DVBStreamType> dvbSubtitles;

    /// Keeps cc708 windows (1-8) for all streams & services
    /// (which ids are the keys).
    QHash<QPair<uint, int>, QHash<int, QStringList> > cc708windows;

    /// Keeps track for decoding time to make timestamps for subtitles.
    int64_t m_curTime;
    int64_t m_curTimeShift;
    uint64_t m_myFramesPlayed;
    bool    m_showProgress;
};

#endif // MYTHCCEXTRACTORPLAYER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
