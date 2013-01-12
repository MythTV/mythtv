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
    friend class FormattedTextSubtitle;

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

    void SetZoom(int percent);
    int GetZoom(void);

    QSize CalcTextSize(const QString &text,
                       const CC708CharacterAttribute &format,
                       float layoutSpacing) const;
    int CalcPadding(const CC708CharacterAttribute &format, bool isLeft) const;

    void RegisterExpiration(MythUIType *shape, long long endTime)
    {
        m_expireTimes.insert(shape, endTime);
    }

    // Temporary method until teletextscreen.cpp is refactored into
    // subtitlescreen.cpp
    static QString GetTeletextFontName(void);

    // MythScreenType methods
    virtual bool Create(void);
    virtual void Pulse(void);

  private:
    void SetElementAdded(void);
    void SetElementResized(void);
    void SetElementDeleted(void);
    void ResetElementState(void);
    void OptimiseDisplayedArea(void);
    void DisplayAVSubtitles(void);
    int  DisplayScaledAVSubtitles(const AVSubtitleRect *rect, QRect &bbox,
                                  bool top, QRect &display, int forced,
                                  QString imagename,
                                  long long displayuntil, long long late);
    void DisplayTextSubtitles(void);
    void DisplayRawTextSubtitles(void);
    void DrawTextSubtitles(QStringList &wrappedsubs, uint64_t start,
                           uint64_t duration);
    void DisplayCC608Subtitles(void);
    void DisplayCC708Subtitles(void);
    void AddScaledImage(QImage &img, QRect &pos);
    void Clear708Cache(int num);
    void InitializeFonts(bool wasResized);
    MythFontProperties* GetFont(CC708CharacterAttribute attr) const;
    void SetFontSize(int pixelSize) { m_fontSize = pixelSize; }

    MythPlayer        *m_player;
    SubtitleReader    *m_subreader;
    CC608Reader       *m_608reader;
    CC708Reader       *m_708reader;
    QRect              m_safeArea;
    QRegExp            m_removeHTML;
    int                m_subtitleType;
    QHash<MythUIType*, long long> m_expireTimes;
    QHash<MythUIType*, MythImage*> m_avsubCache;
    int                m_fontSize;
    int                m_textFontZoom; // valid for 708 & text subs
    int                m_textFontZoomPrev;
    bool               m_refreshModified;
    bool               m_refreshDeleted;
    QHash<int,QList<MythUIType*> > m_708imageCache;
    int                m_fontStretch;
    QString            m_family; // 608, 708, text, teletext
    class SubtitleFormat *m_format;

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
    FormattedTextChunk(const QString &t, CC708CharacterAttribute formatting,
                       SubtitleScreen *p)
        : text(t), format(formatting), parent(p)
    {
    }
    FormattedTextChunk(void) : parent(NULL) {}

    QSize CalcSize(float layoutSpacing = 0.0f) const
    {
        return parent->CalcTextSize(text, format, layoutSpacing);
    }
    int CalcPadding(bool isLeft) const
    {
        return parent->CalcPadding(format, isLeft);
    }
    bool Split(FormattedTextChunk &newChunk);
    QString ToLogString(void) const;

    QString text;
    CC708CharacterAttribute format;
    SubtitleScreen *parent; // where fonts and sizes are kept
};

class FormattedTextLine
{
  public:
    FormattedTextLine(int x = -1, int y = -1, int o_x = -1, int o_y = -1)
        : x_indent(x), y_indent(y), orig_x(o_x), orig_y(o_y) {}

    QSize CalcSize(float layoutSpacing = 0.0f) const
    {
        int height = 0, width = 0;
        int leftPadding = 0, rightPadding = 0;
        QList<FormattedTextChunk>::const_iterator it;
        for (it = chunks.constBegin(); it != chunks.constEnd(); ++it)
        {
            QSize tmp = (*it).CalcSize(layoutSpacing);
            height = max(height, tmp.height());
            width += tmp.width();
            leftPadding = (*it).CalcPadding(true);
            rightPadding = (*it).CalcPadding(false);
            if (it == chunks.constBegin())
                width += leftPadding;
        }
        return QSize(width + rightPadding, height);
    }

    QList<FormattedTextChunk> chunks;
    int x_indent; // -1 means TBD i.e. centered
    int y_indent; // -1 means TBD i.e. relative to bottom
    int orig_x, orig_y; // original, unscaled coordinates
};

class FormattedTextSubtitle
{
  public:
    FormattedTextSubtitle(const QRect &safearea, SubtitleScreen *p)
        : m_safeArea(safearea), parent(p)
    {
        // make cppcheck happy
        m_xAnchorPoint = 0;
        m_yAnchorPoint = 0;
        m_xAnchor = 0;
        m_yAnchor = 0;
    }
    FormattedTextSubtitle(void)
        : m_safeArea(QRect()), parent(NULL)
    {
        // make cppcheck happy
        m_xAnchorPoint = 0;
        m_yAnchorPoint = 0;
        m_xAnchor = 0;
        m_yAnchor = 0;
    }
    void InitFromCC608(vector<CC608Text*> &buffers, int textFontZoom = 100);
    void InitFromCC708(const CC708Window &win, int num,
                       const vector<CC708String*> &list,
                       float aspect = 1.77777f,
                       int textFontZoom = 100);
    void InitFromSRT(QStringList &subs, int textFontZoom);
    void WrapLongLines(void);
    void Layout(void);
    void Layout608(void);
    bool Draw(const QString &base, QList<MythUIType*> *imageCache = NULL,
              uint64_t start = 0, uint64_t duration = 0) const;
    QStringList ToSRT(void) const;
    QRect m_bounds;

  private:
    QVector<FormattedTextLine> m_lines;
    const QRect m_safeArea;
    SubtitleScreen *parent; // where fonts and sizes are kept
    int m_xAnchorPoint; // 0=left, 1=center, 2=right
    int m_yAnchorPoint; // 0=top,  1=center, 2=bottom
    int m_xAnchor; // pixels from left
    int m_yAnchor; // pixels from top
};

#endif // SUBTITLESCREEN_H
