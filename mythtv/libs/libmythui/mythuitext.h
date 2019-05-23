#ifndef MYTHUI_TEXT_H_
#define MYTHUI_TEXT_H_

// QT headers
#include <QTextLayout>
#include <QColor>

// Mythbase headers
#include "mythstorage.h"
#include "mythtypes.h"

// Mythui headers
#include "mythuitype.h"
#include "mythmainwindow.h" // for MythMainWindow::drawRefresh

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
    ~MythUIText();

    void Reset(void) override; // MythUIType
    void ResetMap(const InfoMap &map);

    virtual void SetText(const QString &text);
    QString GetText(void) const { return m_Message; }
    QString GetDefaultText(void) const { return m_DefaultMessage; }

    void SetTextFromMap(const InfoMap &map);

    void SetTemplateText(const QString &text) { m_TemplateText = text; }
    QString GetTemplateText(void) const { return m_TemplateText; }

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

    void SetFontState(const QString&);
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
    const MythFontProperties* GetFontProperties() { return m_Font; }

    void CycleColor(const QColor& startColor, const QColor& endColor, int numSteps);
    void StopCycling();

    int GetJustification(void);
    void SetCutDown(Qt::TextElideMode mode);
    Qt::TextElideMode GetCutDown(void) const { return m_Cutdown; }
    void SetMultiLine(bool multiline);
    bool GetMultiLine(void) const { return m_MultiLine; }

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

    int      m_Justification      {Qt::AlignLeft | Qt::AlignTop};
    MythRect m_OrigDisplayRect;
    MythRect m_AltDisplayRect;
    MythRect m_Canvas;
    MythRect m_drawRect;
    QPoint   m_cursorPos          {-1,-1};

    QString m_Message;
    QString m_CutMessage;
    QString m_DefaultMessage;
    QString m_TemplateText;
    bool    m_TemplateTextFormat;

#if 0 // Not currently used
    bool m_usingAltArea           {false};
#endif
    bool m_ShrinkNarrow           {true};
    Qt::TextElideMode m_Cutdown   {Qt::ElideRight};
    bool m_MultiLine              {false};
    int  m_Ascent                 {0};
    int  m_Descent                {0};
    int  m_leftBearing            {0};
    int  m_rightBearing           {0};
    int  m_Leading                {1};
    int  m_extraLeading           {0};
    int  m_lineHeight             {0};
    int  m_textCursor             {-1};

    QVector<QTextLayout *> m_Layouts;

    MythFontProperties* m_Font    {nullptr};
    FontStates          m_FontStates;

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
    enum Constants {ScrollBounceDelay = MythMainWindow::drawRefresh * 3};
    enum ScrollDir {ScrollNone, ScrollLeft, ScrollRight, ScrollUp, ScrollDown,
                    ScrollHorizontal, ScrollVertical};

    int       m_scrollStartDelay  {ScrollBounceDelay};
    int       m_scrollReturnDelay {ScrollBounceDelay};
    int       m_scrollPause       {0};
    float     m_scrollForwardRate {70.0 / MythMainWindow::drawRefresh};
    float     m_scrollReturnRate  {70.0 / MythMainWindow::drawRefresh};
    bool      m_scrollBounce      {false};
    int       m_scrollOffset      {0};
    float     m_scrollPos         {0};
    int       m_scrollPosWhole    {0};
    ScrollDir m_scrollDirection   {ScrollNone};
    bool      m_scrolling         {false};

    enum TextCase {CaseNormal, CaseUpper, CaseLower, CaseCapitaliseFirst,
                   CaseCapitaliseAll};

    TextCase  m_textCase          {CaseNormal};

    friend class MythUITextEdit;
    friend class MythUIButton;
    friend class MythThemedMenu;
    friend class MythThemedMenuPrivate;
};

#endif
