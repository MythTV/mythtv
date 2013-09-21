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
#include "mythuishape.h"
#include "mythuisimpletext.h"
#include "mythuiimage.h"

class SubtitleScreen;

class FormattedTextChunk
{
public:
    FormattedTextChunk(const QString &t,
                       const CC708CharacterAttribute &formatting,
                       SubtitleScreen *p)
        : text(t), m_format(formatting), parent(p), textFont(NULL) {}
    FormattedTextChunk(void) : parent(NULL), textFont(NULL) {}

    QSize CalcSize(float layoutSpacing = 0.0f) const;
    void CalcPadding(bool isFirst, bool isLast, int &left, int &right) const;
    bool Split(FormattedTextChunk &newChunk);
    QString ToLogString(void) const;
    bool PreRender(bool isFirst, bool isLast, int &x, int y, int height);

    QString text;
    CC708CharacterAttribute m_format; // const
    const SubtitleScreen *parent; // where fonts and sizes are kept

    // The following are calculated by PreRender().
    QString bgShapeName;
    QRect bgShapeRect;
    MythFontProperties *textFont;
    QString textName;
    QRect textRect;
};

class FormattedTextLine
{
public:
    FormattedTextLine(int x = -1, int y = -1, int o_x = -1, int o_y = -1)
        : x_indent(x), y_indent(y), orig_x(o_x), orig_y(o_y) {}

    QSize CalcSize(float layoutSpacing = 0.0f) const;

    QList<FormattedTextChunk> chunks;
    int x_indent; // -1 means TBD i.e. centered
    int y_indent; // -1 means TBD i.e. relative to bottom
    int orig_x, orig_y; // original, unscaled coordinates
};

class FormattedTextSubtitle
{
protected:
    FormattedTextSubtitle(const QString &base, const QRect &safearea,
                          uint64_t start, uint64_t duration,
                          SubtitleScreen *p);
    FormattedTextSubtitle(void);
public:
    virtual ~FormattedTextSubtitle() {}
    // These are the steps that can be done outside of the UI thread
    // and the decoder thread.
    virtual void WrapLongLines(void) {}
    virtual void Layout(void);
    virtual void PreRender(void);
    // This is the step that can only be done in the UI thread.
    virtual void Draw(void);
    virtual int CacheNum(void) const { return -1; }
    QStringList ToSRT(void) const;

protected:
    QString m_base;
    QVector<FormattedTextLine> m_lines;
    const QRect m_safeArea;
    uint64_t m_start;
    uint64_t m_duration;
    SubtitleScreen *m_subScreen; // where fonts and sizes are kept
    int m_xAnchorPoint; // 0=left, 1=center, 2=right
    int m_yAnchorPoint; // 0=top,  1=center, 2=bottom
    int m_xAnchor; // pixels from left
    int m_yAnchor; // pixels from top
    QRect m_bounds;
};

class FormattedTextSubtitleSRT : public FormattedTextSubtitle
{
public:
    FormattedTextSubtitleSRT(const QString &base,
                             const QRect &safearea,
                             uint64_t start,
                             uint64_t duration,
                             SubtitleScreen *p,
                             const QStringList &subs) :
        FormattedTextSubtitle(base, safearea, start, duration, p)
    {
        Init(subs);
    }
    virtual void WrapLongLines(void);
private:
    void Init(const QStringList &subs);
};

class FormattedTextSubtitle608 : public FormattedTextSubtitle
{
public:
    FormattedTextSubtitle608(const vector<CC608Text*> &buffers,
                             const QString &base = "",
                             const QRect &safearea = QRect(),
                             SubtitleScreen *p = NULL) :
        FormattedTextSubtitle(base, safearea, 0, 0, p)
    {
        Init(buffers);
    }
    virtual void Layout(void);
private:
    void Init(const vector<CC608Text*> &buffers);
};

class FormattedTextSubtitle708 : public FormattedTextSubtitle
{
public:
    FormattedTextSubtitle708(const CC708Window &win,
                             int num,
                             const vector<CC708String*> &list,
                             const QString &base = "",
                             const QRect &safearea = QRect(),
                             SubtitleScreen *p = NULL,
                             float aspect = 1.77777f) :
        FormattedTextSubtitle(base, safearea, 0, 0, p),
        m_num(num),
        m_bgFillAlpha(win.GetFillAlpha()),
        m_bgFillColor(win.GetFillColor())
    {
        Init(win, list, aspect);
    }
    virtual void Draw(void);
    virtual int CacheNum(void) const { return m_num; }
private:
    void Init(const CC708Window &win,
              const vector<CC708String*> &list,
              float aspect);
    int m_num;
    uint m_bgFillAlpha;
    QColor m_bgFillColor;
};

class SubtitleScreen : public MythScreenType
{
public:
    SubtitleScreen(MythPlayer *player, const char * name, int fontStretch);
    virtual ~SubtitleScreen();

    void EnableSubtitles(int type, bool forced_only = false);
    void DisableForcedSubtitles(void);
    int  EnabledSubtitleType(void) { return m_subtitleType; }

    void ClearAllSubtitles(void);
    void ClearNonDisplayedSubtitles(void);
    void ClearDisplayedSubtitles(void);
    void DisplayDVDButton(AVSubtitle* dvdButton, QRect &buttonPos);

    void SetZoom(int percent);
    int GetZoom(void) const;
    void SetDelay(int ms);
    int GetDelay(void) const;

    class SubtitleFormat *GetSubtitleFormat(void) { return m_format; }
    void Clear708Cache(uint64_t mask);
    void SetElementAdded(void);
    void SetElementResized(void);
    void SetElementDeleted(void);

    QSize CalcTextSize(const QString &text,
                       const CC708CharacterAttribute &format,
                       float layoutSpacing) const;
    void CalcPadding(const CC708CharacterAttribute &format,
                     bool isFirst, bool isLast,
                     int &left, int &right) const;
    MythFontProperties* GetFont(const CC708CharacterAttribute &attr) const;
    void SetFontSize(int pixelSize) { m_fontSize = pixelSize; }

    // Temporary method until teletextscreen.cpp is refactored into
    // subtitlescreen.cpp
    static QString GetTeletextFontName(void);

    // MythScreenType methods
    virtual bool Create(void);
    virtual void Pulse(void);

private:
    void ResetElementState(void);
    void OptimiseDisplayedArea(void);
    void DisplayAVSubtitles(void);
    int  DisplayScaledAVSubtitles(const AVSubtitleRect *rect, QRect &bbox,
                                  bool top, QRect &display, int forced,
                                  QString imagename,
                                  long long displayuntil, long long late);
    void DisplayTextSubtitles(void);
    void DisplayRawTextSubtitles(void);
    void DrawTextSubtitles(const QStringList &subs, uint64_t start,
                           uint64_t duration);
    void DisplayCC608Subtitles(void);
    void DisplayCC708Subtitles(void);
    void AddScaledImage(QImage &img, QRect &pos);
    void InitializeFonts(bool wasResized);

    MythPlayer        *m_player;
    SubtitleReader    *m_subreader;
    CC608Reader       *m_608reader;
    CC708Reader       *m_708reader;
    QRect              m_safeArea;
    QRegExp            m_removeHTML;
    int                m_subtitleType;
    int                m_fontSize;
    int                m_textFontZoom; // valid for 708 & text subs
    int                m_textFontZoomPrev;
    int                m_textFontDelayMs; // valid for text subs
    int                m_textFontDelayMsPrev;
    bool               m_refreshModified;
    bool               m_refreshDeleted;
    int                m_fontStretch;
    QString            m_family; // 608, 708, text, teletext
    // Subtitles initialized but still to be processed and drawn
    QList<FormattedTextSubtitle *> m_qInited;
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

#endif // SUBTITLESCREEN_H
