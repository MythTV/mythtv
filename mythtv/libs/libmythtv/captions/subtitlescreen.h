// -*- Mode: c++ -*-

#ifndef SUBTITLESCREEN_H
#define SUBTITLESCREEN_H

#include <utility>
#ifdef USING_LIBASS
extern "C" {
#include <ass/ass.h>
}
#endif

// Qt headers
#include <QFont>
#include <QHash>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <QVector>

// MythTV headers
#include "libmythtv/captions/subtitlereader.h"
#include "libmythtv/mythplayer.h"
#include "libmythui/mythscreentype.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuishape.h"
#include "libmythui/mythuisimpletext.h"

class SubtitleScreen;
class TestSubtitleScreen;

class FormattedTextChunk
{
public:
    FormattedTextChunk(QString t,
                       CC708CharacterAttribute formatting,
                       SubtitleScreen *p)
        // NOLINTNEXTLINE(performance-move-const-arg)
        : m_text(std::move(t)), m_format(std::move(formatting)), m_parent(p) {}
    FormattedTextChunk(void) = default;

    QSize CalcSize(float layoutSpacing = 0.0F) const;
    void CalcPadding(bool isFirst, bool isLast, int &left, int &right) const;
    bool Split(FormattedTextChunk &newChunk);
    QString ToLogString(void) const;
    bool PreRender(bool isFirst, bool isLast, int &x, int y, int height);

    QString               m_text;
    CC708CharacterAttribute m_format; // const
    const SubtitleScreen *m_parent {nullptr}; // where fonts and sizes are kept

    // The following are calculated by PreRender().
    QString               m_bgShapeName;
    QRect                 m_bgShapeRect;
    MythFontProperties   *m_textFont {nullptr};
    QString               m_textName;
    QRect                 m_textRect;
};

class FormattedTextLine
{
public:
    explicit FormattedTextLine(int x = -1, int y = -1, int o_x = -1, int o_y = -1)
        : m_xIndent(x), m_yIndent(y), m_origX(o_x), m_origY(o_y) {}

    QSize CalcSize(float layoutSpacing = 0.0F) const;

    QList<FormattedTextChunk> chunks;
    int m_xIndent {-1}; // -1 means TBD i.e. centered
    int m_yIndent {-1}; // -1 means TBD i.e. relative to bottom
    int m_origX   {-1}; // original, unscaled coordinates
    int m_origY   {-1}; // original, unscaled coordinates
};

class MTV_PUBLIC FormattedTextSubtitle
{
    friend class TestSubtitleScreen;

protected:
    FormattedTextSubtitle(QString base, QRect safearea,
                          std::chrono::milliseconds start,
                          std::chrono::milliseconds duration,
                          SubtitleScreen *p) :
        m_base(std::move(base)), m_safeArea(safearea),
        m_start(start), m_duration(duration), m_subScreen(p) {}
    FormattedTextSubtitle(void) = default;
public:
    virtual ~FormattedTextSubtitle() = default;
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
    QString         m_base;
    QVector<FormattedTextLine> m_lines;
    const QRect     m_safeArea;
    std::chrono::milliseconds m_start    {0ms};
    std::chrono::milliseconds m_duration {0ms};
    SubtitleScreen *m_subScreen    {nullptr}; // where fonts and sizes are kept
    int             m_xAnchorPoint {0}; // 0=left, 1=center, 2=right
    int             m_yAnchorPoint {0}; // 0=top,  1=center, 2=bottom
    int             m_xAnchor      {0}; // pixels from left
    int             m_yAnchor      {0}; // pixels from top
    QRect           m_bounds;
};

class MTV_PUBLIC FormattedTextSubtitleSRT : public FormattedTextSubtitle
{
public:
    FormattedTextSubtitleSRT(const QString &base,
                             QRect safearea,
                             std::chrono::milliseconds start,
                             std::chrono::milliseconds duration,
                             SubtitleScreen *p,
                             const QStringList &subs) :
        FormattedTextSubtitle(base, safearea, start, duration, p)
    {
        Init(subs);
    }
    void WrapLongLines(void) override; // FormattedTextSubtitle
private:
    void Init(const QStringList &subs);
};

class MTV_PUBLIC FormattedTextSubtitle608 : public FormattedTextSubtitle
{
public:
    explicit FormattedTextSubtitle608(const std::vector<CC608Text*> &buffers,
                             const QString &base = "",
                             QRect safearea = QRect(),
                             SubtitleScreen *p = nullptr) :
        FormattedTextSubtitle(base, safearea, 0ms, 0ms, p)
    {
        Init(buffers);
    }
    void Layout(void) override; // FormattedTextSubtitle
private:
    void Init(const std::vector<CC608Text*> &buffers);
};

class MTV_PUBLIC FormattedTextSubtitle708 : public FormattedTextSubtitle
{
public:
    FormattedTextSubtitle708(const CC708Window &win,
                             int num,
                             const std::vector<CC708String*> &list,
                             const QString &base = "",
                             QRect safearea = QRect(),
                             SubtitleScreen *p = nullptr,
                             float aspect = 1.77777F) :
        FormattedTextSubtitle(base, safearea, 0ms, 0ms, p),
        m_num(num),
        m_bgFillAlpha(win.GetFillAlpha()),
        m_bgFillColor(win.GetFillColor())
    {
        Init(win, list, aspect);
    }
    void Draw(void) override; // FormattedTextSubtitle
    int CacheNum(void) const override // FormattedTextSubtitle
        { return m_num; }
private:
    void Init(const CC708Window &win,
              const std::vector<CC708String*> &list,
              float aspect);
    int m_num;
    uint m_bgFillAlpha;
    QColor m_bgFillColor;
};

class MTV_PUBLIC SubtitleScreen : public MythScreenType
{
    Q_OBJECT
public:
    SubtitleScreen(MythPlayer* Player, MythPainter* Painter, const QString& Name, int FontStretch);
   ~SubtitleScreen() override;

    void EnableSubtitles(int type, bool forced_only = false);
    void DisableForcedSubtitles(void);
    int  EnabledSubtitleType(void) const { return m_subtitleType; }

    void ClearAllSubtitles(void);
    void ClearNonDisplayedSubtitles(void);
    void ClearDisplayedSubtitles(void);
    void DisplayDVDButton(AVSubtitle* dvdButton, QRect &buttonPos);

    void SetZoom(int percent);
    int GetZoom(void) const;
    void SetDelay(std::chrono::milliseconds ms);
    std::chrono::milliseconds GetDelay(void) const;

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

    // Temporary methods until teletextscreen.cpp is refactored into
    // subtitlescreen.cpp
    static QString GetTeletextFontName(void);
    static int GetTeletextBackgroundAlpha(void);

    // MythScreenType methods
    bool Create(void) override; // MythScreenType
    void Pulse(void) override; // MythUIType

private:
    void ResetElementState(void);
    void OptimiseDisplayedArea(void);
    void DisplayAVSubtitles(void);
    int  DisplayScaledAVSubtitles(const AVSubtitleRect *rect, QRect &bbox,
                                  bool top, QRect &display, int forced,
                                  const QString& imagename,
                                  std::chrono::milliseconds displayuntil,
                                  std::chrono::milliseconds late);
    void DisplayTextSubtitles(void);
    void DisplayRawTextSubtitles(void);
    void DrawTextSubtitles(const QStringList &subs, std::chrono::milliseconds start,
                           std::chrono::milliseconds duration);
    void DisplayCC608Subtitles(void);
    void DisplayCC708Subtitles(void);
    void AddScaledImage(QImage &img, QRect &pos);
    void InitializeFonts(bool wasResized);

    MythPlayer     *m_player              {nullptr};
    SubtitleReader *m_subreader           {nullptr};
    CC608Reader    *m_cc608reader         {nullptr};
    CC708Reader    *m_cc708reader         {nullptr};
    QRect           m_safeArea;
    int             m_subtitleType        {kDisplayNone};
    int             m_fontSize            {0};
    int             m_textFontZoom        {100}; // valid for 708 & text subs
    int             m_textFontZoomPrev    {100};
    std::chrono::milliseconds m_textFontDelayMs     {0ms}; // valid for text subs
    std::chrono::milliseconds m_textFontDelayMsPrev {0ms};
    std::chrono::milliseconds m_textFontMinDurationMs           {50ms};
    std::chrono::milliseconds m_textFontMinDurationMsPrev       {50ms};
    std::chrono::milliseconds m_textFontDurationExtensionMs     {0ms};
    std::chrono::milliseconds m_textFontDurationExtensionMsPrev {0ms};
    bool            m_refreshModified     {false};
    bool            m_refreshDeleted      {false};
    bool            m_atEnd               {false};
    int             m_fontStretch;
    QString         m_family; // 608, 708, text, teletext
    // Subtitles initialized but still to be processed and drawn
    QList<FormattedTextSubtitle *> m_qInited;
    class SubtitleFormat *m_format        {nullptr};

#ifdef USING_LIBASS
    bool InitialiseAssLibrary(void);
    void LoadAssFonts(void);
    void CleanupAssLibrary(void);
    void InitialiseAssTrack(int tracknum);
    void CleanupAssTrack(void);
    void AddAssEvent(char *event, uint32_t starttime, uint32_t endtime);
    void ResizeAssRenderer(void);
    void RenderAssTrack(std::chrono::milliseconds timecode, bool force);

    ASS_Library    *m_assLibrary          {nullptr};
    ASS_Renderer   *m_assRenderer         {nullptr};
    int             m_assTrackNum         {-1};
    ASS_Track      *m_assTrack            {nullptr};
    uint            m_assFontCount        {0};
#endif // USING_LIBASS
};

#endif // SUBTITLESCREEN_H
