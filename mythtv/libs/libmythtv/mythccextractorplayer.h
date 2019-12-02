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

enum sub_types
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
    int64_t start_time;
    /// Time we have to show subtitle, msec.
    int length;
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

    static const int kDefaultLength;
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
    QHash<int, SRTWriter*> srtwriters;
    QHash<int,int>         subs_num;
};

class CC608Stuff : public SRTStuff
{
  public:
    CC608Stuff() : reader(nullptr) { }
    ~CC608Stuff();
    CC608Reader *reader;
    CC608StreamType subs;
};
using CC608Info = QHash<uint, CC608Stuff>;

class CC708Stuff : public SRTStuff
{
  public:
    CC708Stuff() : reader(nullptr) { }
    ~CC708Stuff();
    CC708Reader *reader;
    CC708StreamType subs;
};
using CC708Info = QHash<uint, CC708Stuff>;

class TeletextExtractorReader;
class TeletextStuff : public SRTStuff
{
  public:
    TeletextStuff() : reader(nullptr) { }
    ~TeletextStuff();
    TeletextExtractorReader *reader;
    TeletextStreamType subs;
};
using TeletextInfo = QHash<uint, TeletextStuff>;

class DVBSubStuff
{
  public:
    DVBSubStuff() : reader(nullptr), subs_num(0) { }
    ~DVBSubStuff();
    SubtitleReader *reader;
    int             subs_num;
    DVBStreamType   subs;
};
using DVBSubInfo = QHash<uint, DVBSubStuff>;

using SubtitleReaders = QHash<uint, SubtitleReader*>;

class MTV_PUBLIC MythCCExtractorPlayer : public MythPlayer
{
  public:
    MythCCExtractorPlayer(PlayerFlags flags, bool showProgress,
                          QString fileName, const QString & destdir);
    MythCCExtractorPlayer(const MythCCExtractorPlayer& rhs);
    ~MythCCExtractorPlayer() = default;

    bool run(void);

    CC708Reader    *GetCC708Reader(uint id=0) override; // MythPlayer
    CC608Reader    *GetCC608Reader(uint id=0) override; // MythPlayer
    SubtitleReader *GetSubReader(uint id=0) override; // MythPlayer
    TeletextReader *GetTeletextReader(uint id=0) override; // MythPlayer

  private:
    void IngestSubtitle(QList<OneSubtitle>&, const QStringList&);
    static void IngestSubtitle(QList<OneSubtitle>&, const OneSubtitle&);

    enum { kProcessNormal = 0, kProcessFinalize = 1 };
    void Ingest608Captions(void);
    void Process608Captions(uint flags);

    void Ingest708Captions(void);
    void Ingest708Caption(uint streamId, uint serviceIdx, uint windowIdx,
                          uint start_row, uint start_column,
                          const CC708Window &win,
                          const vector<CC708String*> &content);
    void Process708Captions(uint flags);

    void IngestTeletext(void);
    void ProcessTeletext(uint flags);

    void IngestDVBSubtitles(void);
    void ProcessDVBSubtitles(uint flags);

    void OnGotNewFrame(void);

  protected:
    CC608Info       m_cc608_info;
    CC708Info       m_cc708_info;
    TeletextInfo    m_ttx_info;
    DVBSubInfo      m_dvbsub_info;

    /// Keeps cc708 windows (1-8) for all streams & services
    /// (which ids are the keys).
    class Window
    {
      public:
        uint row;
        uint column;
        QStringList text;
    };
    using WindowsOnService = QHash<uint, QMap<int, Window> >;
    QHash<uint, WindowsOnService > m_cc708_windows;

    /// Keeps track for decoding time to make timestamps for subtitles.
    double  m_curTime;
    uint64_t m_myFramesPlayed;
    bool    m_showProgress;
    QString m_fileName;
    QDir    m_workingDir;
    QString m_baseName;
};

#endif // MYTHCCEXTRACTORPLAYER_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
