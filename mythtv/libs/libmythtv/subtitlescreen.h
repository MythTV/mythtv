// -*- Mode: c++ -*-

#ifndef SUBTITLESCREEN_H
#define SUBTITLESCREEN_H

#include <QStringList>
#include <QRegExp>
#include <QVector>
#include <QFont>
#include <QHash>
#include <QRect>
#include <QSize>

#ifdef USING_LIBASS
extern "C" {
#include <ass/ass.h>
}
#endif

#include "mythscreentype.h"
#include "subtitlereader.h"
#include "mythplayer.h"

class SubtitleScreen : public MythScreenType
{
    static bool InitialiseFont(int fontStretch = QFont::Unstretched);
    static bool Initialise708Fonts(int fontStretch = QFont::Unstretched);

  public:
    SubtitleScreen(MythPlayer *player, const char * name, int fontStretch);
    virtual ~SubtitleScreen();

    void EnableSubtitles(int type, bool forced_only = false);
    void DisableForcedSubtitles(void);
    int  EnabledSubtitleType(void) { return m_subtitleType; }

    void ClearAllSubtitles(void);
    void ClearNonDisplayedSubtitles(void);
    void ClearDisplayedSubtitles(void);
    void ExpireSubtitles(void);
    void DisplayDVDButton(AVSubtitle* dvdButton, QRect &buttonPos);

    void RegisterExpiration(MythUIType *shape, long long endTime)
    {
        m_expireTimes.insert(shape, endTime);
    }

    bool GetUseBackground(void) { return m_useBackground; }

    // MythScreenType methods
    virtual bool Create(void);
    virtual void Pulse(void);

  private:
    void OptimiseDisplayedArea(void);
    void DisplayAVSubtitles(void);
    void DisplayTextSubtitles(void);
    void DisplayRawTextSubtitles(void);
    void DrawTextSubtitles(QStringList &wrappedsubs, uint64_t start,
                           uint64_t duration);
    void DisplayCC608Subtitles(void);
    void DisplayCC708Subtitles(void);
    void AddScaledImage(QImage &img, QRect &pos);
    void Clear708Cache(int num);
    void Display708Strings(const CC708Window &win, int num,
                           float aspect, vector<CC708String*> &list);
    MythFontProperties* Get708Font(CC708CharacterAttribute attr);

    MythPlayer        *m_player;
    SubtitleReader    *m_subreader;
    CC608Reader       *m_608reader;
    CC708Reader       *m_708reader;
    QRect              m_safeArea;
    bool               m_useBackground;
    QRegExp            m_removeHTML;
    int                m_subtitleType;
    QHash<MythUIType*, long long> m_expireTimes;
    int                m_708fontSizes[3];
    int                m_textFontZoom;
    bool               m_refreshArea;
    QHash<int,QList<MythUIType*> > m_708imageCache;
    int                m_fontStretch;

#ifdef USING_LIBASS
    bool InitialiseAssLibrary(void);
    void LoadAssFonts(void);
    void CleanupAssLibrary(void);
    void InitialiseAssTrack(int tracknum);
    void CleanupAssTrack(void);
    void AddAssEvent(char *event);
    void ResizeAssRenderer(void);
    void RenderAssTrack(uint64_t timecode);

    ASS_Library       *m_assLibrary;
    ASS_Renderer      *m_assRenderer;
    int                m_assTrackNum;
    ASS_Track         *m_assTrack;
    uint               m_assFontCount;
#endif // USING_LIBASS
};

class FormattedTextChunk
{
  public:
    FormattedTextChunk(
        const QString &t, bool ital, bool bold, bool uline, QColor clr) :
        text(t), isItalic(ital), isBold(bold), isUnderline(uline), color(clr)
    {
    }
    FormattedTextChunk(void) {}

    QSize CalcSize(int pixelSize) const;
    bool Split(FormattedTextChunk &newChunk);

    QString text;
    bool isItalic;
    bool isBold;
    bool isUnderline;
    QColor color;
};

class FormattedTextLine
{
  public:
    FormattedTextLine(int x = -1, int y = -1) : x_indent(x), y_indent(y) {}

    QSize CalcSize(int pixelSize) const
    {
        int height = 0, width = 0;
        QList<FormattedTextChunk>::const_iterator it;
        for (it = chunks.constBegin(); it != chunks.constEnd(); ++it)
        {
            QSize tmp = (*it).CalcSize(pixelSize);
            height = max(height, tmp.height());
            width += tmp.width();
        }
        return QSize(width, height);
    }

    QList<FormattedTextChunk> chunks;
    int x_indent; // -1 means TBD i.e. centered
    int y_indent; // -1 means TBD i.e. relative to bottom
};

class FormattedTextSubtitle
{
  public:
    FormattedTextSubtitle(const QRect &safearea) :
        m_safeArea(safearea), m_useBackground(true) {}
    void InitFromCC608(vector<CC608Text*> &buffers);
    void InitFromSRT(QStringList &subs, int textFontZoom);
    void WrapLongLines(void);
    void Draw(SubtitleScreen *parent,
              uint64_t start = 0, uint64_t duration = 0) const;

  private:
    QVector<FormattedTextLine> m_lines;
    const QRect m_safeArea;
    bool m_useBackground;
    int m_pixelSize;
};

#endif // SUBTITLESCREEN_H
