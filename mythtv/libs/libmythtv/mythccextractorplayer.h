// -*- Mode: c++ -*-

#ifndef MYTHCCEXTRACTORPLAYER_H
#define MYTHCCEXTRACTORPLAYER_H

#include <cstdint>

#include <QStringList>
#include <QImage>
#include <QPoint>
#include <QHash>
#include <QDir>

#include "mythplayer.h"

class SRTWriter;

enum sub_types : std::uint8_t
{
    kSubCC608,
    kSubCC708,
    kSubDVB,
    kSubTeletext,
};

/**
 * Represents one subtitle record.
 */
class MTV_PUBLIC OneSubtitle
{
  public:
    /// Time we have to start showing subtitle, msec.
    std::chrono::milliseconds m_startTime { 0 };
    /// Time we have to show subtitle, msec.
    std::chrono::milliseconds m_length { -1ms };
    /// Is this a text subtitle.
    bool m_isText { true };
    /// Lines of text of subtitles.
    QStringList m_text;
    /// Image of subtitle.
    QImage m_img;
    /// Shift of image on the screen.
    QPoint m_imgShift { 0,0 };

    OneSubtitle() = default;

    static constexpr std::chrono::milliseconds kDefaultLength { 750ms };
};

/// Key is a CC number (1-4), values are the subtitles in chrono order.
using CC608StreamType = QHash<int, QList<OneSubtitle> >;
/// Key is a CC service (1-63), values are the subtitles in chrono order.
using CC708StreamType = QHash<int, QList<OneSubtitle> >;
/// Key is a page number, values are the subtitles in chrono order.
using TeletextStreamType = QHash<int, QList<OneSubtitle> >;
/// Subtitles in chrono order.
using DVBStreamType = QList<OneSubtitle>;

class SRTStuff
{
  public:
    SRTStuff() = default;
    virtual ~SRTStuff();
    QHash<int, SRTWriter*> m_srtWriters;
    QHash<int,int>         m_subsNum;
};

class CC608Stuff : public SRTStuff
{
  public:
    CC608Stuff() = default;
    ~CC608Stuff() override;
    CC608Reader     *m_reader { nullptr };
    CC608StreamType  m_subs;
};
using CC608Info = QHash<uint, CC608Stuff>;

class CC708Stuff : public SRTStuff
{
  public:
    CC708Stuff() = default;
    ~CC708Stuff() override;
    CC708Reader     *m_reader { nullptr };
    CC708StreamType  m_subs;
};
using CC708Info = QHash<uint, CC708Stuff>;

class TeletextExtractorReader;
class TeletextStuff : public SRTStuff
{
  public:
    TeletextStuff() = default;
    ~TeletextStuff() override;
    TeletextExtractorReader *m_reader { nullptr };
    TeletextStreamType       m_subs;
};
using TeletextInfo = QHash<uint, TeletextStuff>;

class DVBSubStuff
{
  public:
    DVBSubStuff() = default;
    ~DVBSubStuff();
    SubtitleReader *m_reader  { nullptr };
    int             m_subsNum {       0 };
    DVBStreamType   m_subs;
};
using DVBSubInfo = QHash<uint, DVBSubStuff>;

using SubtitleReaders = QHash<uint, SubtitleReader*>;

class MTV_PUBLIC MythCCExtractorPlayer : public MythPlayer
{
  public:
    MythCCExtractorPlayer(PlayerContext* Context, PlayerFlags flags, bool showProgress,
                          QString fileName, const QString & destdir);
    MythCCExtractorPlayer(const MythCCExtractorPlayer& rhs);
    ~MythCCExtractorPlayer() override = default;

    bool run(void);

    CC708Reader    *GetCC708Reader(uint id=0) override; // MythPlayer
    CC608Reader    *GetCC608Reader(uint id=0) override; // MythPlayer
    SubtitleReader *GetSubReader(uint id=0) override; // MythPlayer
    TeletextReader *GetTeletextReader(uint id=0) override; // MythPlayer

  private:
    void IngestSubtitle(QList<OneSubtitle> &list, const QStringList &content) const;
    static void IngestSubtitle(QList<OneSubtitle> &list, const OneSubtitle &content);

    enum : std::uint8_t { kProcessNormal = 0, kProcessFinalize = 1 };
    void Ingest608Captions(void);
    void Process608Captions(uint flags);

    void Ingest708Captions(void);
    void Ingest708Caption(uint streamId, uint serviceIdx, uint windowIdx,
                          uint start_row, uint start_column,
                          const CC708Window &win,
                          const std::vector<CC708String*> &content);
    void Process708Captions(uint flags);

    void IngestTeletext(void);
    void ProcessTeletext(uint flags);

    void IngestDVBSubtitles(void);
    void ProcessDVBSubtitles(uint flags);

    void OnGotNewFrame(void);

  protected:
    CC608Info       m_cc608Info;
    CC708Info       m_cc708Info;
    TeletextInfo    m_ttxInfo;
    DVBSubInfo      m_dvbsubInfo;

    /// Keeps cc708 windows (1-8) for all streams & services
    /// (which ids are the keys).
    class Window
    {
      public:
        uint row    {0};
        uint column {0};
        QStringList text;
    };
    using WindowsOnService = QHash<uint, QMap<int, Window> >;
    QHash<uint, WindowsOnService > m_cc708Windows;

    /// Keeps track for decoding time to make timestamps for subtitles.
    std::chrono::milliseconds  m_curTime {0ms};
    uint64_t m_myFramesPlayed {0};
    bool    m_showProgress    {false};
    QString m_fileName;
    QDir    m_workingDir;
    QString m_baseName;
};

#endif // MYTHCCEXTRACTORPLAYER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
