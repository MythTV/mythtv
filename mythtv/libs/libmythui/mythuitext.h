#ifndef MYTHUI_TEXT_H_
#define MYTHUI_TEXT_H_

// QT headers
#include <QTextLayout>
#include <QColor>

// MythTV
#include "libmythbase/mythstorage.h"
#include "libmythbase/mythtypes.h"
#include "libmythui/mythuitype.h"
#include "libmythui/mythmainwindow.h" // for MythMainWindow::drawRefresh

static constexpr uint8_t DEFAULT_REFRESH_RATE { 70 }; // Hz

class MythFontProperties;

/**
 *  \class MythUIText
 *
 *  \brief All purpose text widget, displays a text string
 *
 *  Font, alignment, scrolling and color cycling effects may be applied to
 *  the text in this widget.
 *
 *  \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIText : public MythUIType, public StorageUser
{
    using FontStates = QMap<QString, MythFontProperties>;

  public:
    MythUIText(MythUIType *parent, const QString &name);
    MythUIText(const QString &text, const MythFontProperties &font,
               QRect displayRect, QRect altDisplayRect,
               MythUIType *parent, const QString &name);
    ~MythUIText() override;

    void Reset(void) override; // MythUIType
    void ResetMap(const InfoMap &map);

    virtual void SetText(const QString &text);
    QString GetText(void) const { return m_message; }
    QString GetDefaultText(void) const { return m_defaultMessage; }

    void SetTextFromMap(const InfoMap &map);

    void SetTemplateText(const QString &text) { m_templateText = text; }
    QString GetTemplateText(void) const { return m_templateText; }

#if 0 // Not currently used
    void UseAlternateArea(bool useAlt);
#endif

    void Pulse(void) override; // MythUIType
    QPoint CursorPosition(int text_offset);
    int MoveCursor(int lines);

    // StorageUser
    void SetDBValue(const QString &text) override // StorageUser
        { SetText(text); }
    QString GetDBValue(void) const override // StorageUser
        { return GetText(); }

    void SetFontState(const QString &state);
    void SetJustification(int just);

  protected:
    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect) override; // MythUIType

    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType

    void SetFontProperties(const MythFontProperties &fontProps);
    const MythFontProperties* GetFontProperties() { return m_font; }

    void CycleColor(const QColor& startColor, const QColor& endColor, int numSteps);
    void StopCycling();

    int GetJustification(void) const;
    void SetCutDown(Qt::TextElideMode mode);
    Qt::TextElideMode GetCutDown(void) const { return m_cutdown; }
    void SetMultiLine(bool multiline);
    bool GetMultiLine(void) const { return m_multiLine; }

    void SetArea(const MythRect &rect) override; // MythUIType
    void SetPosition(const MythPoint &pos) override; // MythUIType
    MythRect GetDrawRect(void) { return m_drawRect; }

    void SetCanvasPosition(int x, int y);
    void ShiftCanvas(int x, int y);

    bool FormatTemplate(QString & paragraph, QTextLayout *layout);
    bool Layout(QString & paragraph, QTextLayout *layout, bool final,
                bool & overflow, qreal width, qreal & height, bool force,
                qreal & last_line_width, QRectF & min_rect, int & num_lines);
    bool LayoutParagraphs(const QStringList & paragraphs,
                          const QTextOption & textoption,
                          qreal width, qreal & height, QRectF & min_rect,
                          qreal & last_line_width, int & num_lines, bool final);
    bool GetNarrowWidth(const QStringList & paragraphs,
                        const QTextOption & textoption, qreal & width);
    void FillCutMessage(void);

    int      m_justification      {Qt::AlignLeft | Qt::AlignTop};
    MythRect m_origDisplayRect;
    MythRect m_altDisplayRect;
    MythRect m_canvas;
    MythRect m_drawRect;
    QPoint   m_cursorPos          {-1,-1};

    QString m_message;
    QString m_cutMessage;
    QString m_defaultMessage;
    QString m_templateText;

#if 0 // Not currently used
    bool m_usingAltArea           {false};
#endif
    bool m_shrinkNarrow           {true};
    Qt::TextElideMode m_cutdown   {Qt::ElideRight};
    bool m_multiLine              {false};
    int  m_ascent                 {0};
    int  m_descent                {0};
    int  m_leftBearing            {0};
    int  m_rightBearing           {0};
    int  m_leading                {1};
    int  m_extraLeading           {0};
    int  m_lineHeight             {0};
    int  m_textCursor             {-1};

    QVector<QTextLayout *> m_layouts;

    MythFontProperties* m_font    {nullptr};
    FontStates          m_fontStates;

    bool   m_colorCycling         {false};
    QColor m_startColor;
    QColor m_endColor;
    int    m_numSteps             {0};
    int    m_curStep              {0};
    float  m_curR                 {0.0};
    float  m_curG                 {0.0};
    float  m_curB                 {0.0};
    float  m_incR                 {0.0};
    float  m_incG                 {0.0};
    float  m_incB                 {0.0};

    // Default delay of 3 seconds before 'bouncing' the scrolling text
    enum Constants : std::uint8_t
                   {ScrollBounceDelay = DEFAULT_REFRESH_RATE * 3};
    enum ScrollDir : std::uint8_t
                   {ScrollNone, ScrollLeft, ScrollRight, ScrollUp, ScrollDown,
                    ScrollHorizontal, ScrollVertical};

    int       m_scrollStartDelay  {ScrollBounceDelay};
    int       m_scrollReturnDelay {ScrollBounceDelay};
    float     m_scrollPause       {0.0};
    float     m_scrollForwardRate {1.0};
    float     m_scrollReturnRate  {1.0};
    bool      m_scrollBounce      {false};
    int       m_scrollOffset      {0};
    float     m_scrollPos         {0};
    int       m_scrollPosWhole    {0};
    ScrollDir m_scrollDirection   {ScrollNone};
    bool      m_scrolling         {false};
    int64_t   m_lastUpdate        {QDateTime::currentMSecsSinceEpoch()};

    enum TextCase : std::uint8_t
                  {CaseNormal, CaseUpper, CaseLower, CaseCapitaliseFirst,
                   CaseCapitaliseAll};

    TextCase  m_textCase          {CaseNormal};

    friend class MythUITextEdit;
    friend class MythUIButton;
    friend class MythThemedMenu;
    friend class MythThemedMenuPrivate;
};

#endif
