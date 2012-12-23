#ifndef MYTHUI_TEXT_H_
#define MYTHUI_TEXT_H_

// QT headers
#include <QTextLayout>
#include <QColor>

// Mythdb headers
#include "mythstorage.h"

// Mythui headers
#include "mythuitype.h"
#include "mythmainwindow.h"

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
  public:
    MythUIText(MythUIType *parent, const QString &name);
    MythUIText(const QString &text, const MythFontProperties &font,
               QRect displayRect, QRect altDisplayRect,
               MythUIType *parent, const QString &name);
    ~MythUIText();

    void Reset(void);

    virtual void SetText(const QString &text);
    QString GetText(void) const;
    QString GetDefaultText(void) const;

    void SetTextFromMap(QHash<QString, QString> &map);

    void SetTemplateText(const QString &text) { m_TemplateText = text; }
    QString GetTemplateText(void) const { return m_TemplateText; }

#if 0 // Not currently used
    void UseAlternateArea(bool useAlt);
#endif

    virtual void Pulse(void);
    QPoint CursorPosition(int text_offset);

    // StorageUser
    void SetDBValue(const QString &text) { SetText(text); }
    QString GetDBValue(void) const { return GetText(); }

    void SetFontState(const QString&);
    void SetJustification(int just);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void SetFontProperties(const MythFontProperties &fontProps);
    const MythFontProperties* GetFontProperties() { return m_Font; }

    void CycleColor(QColor startColor, QColor endColor, int numSteps);
    void StopCycling();

    int GetJustification(void);
    void SetCutDown(Qt::TextElideMode mode);
    Qt::TextElideMode GetCutDown(void) const { return m_Cutdown; }
    void SetMultiLine(bool multiline);
    bool GetMultiLine(void) const { return m_MultiLine; }

    void SetArea(const MythRect &rect);
    void SetPosition(const MythPoint &pos);
    MythRect GetDrawRect(void) { return m_drawRect; }

    void SetCanvasPosition(int x, int y);
    void ShiftCanvas(int x, int y);

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

    int m_Justification;
    MythRect m_OrigDisplayRect;
    MythRect m_AltDisplayRect;
    MythRect m_Canvas;
    MythRect m_drawRect;
    QPoint   m_cursorPos;

    QString m_Message;
    QString m_CutMessage;
    QString m_DefaultMessage;
    QString m_TemplateText;

#if 0 // Not currently used
    bool m_usingAltArea;
#endif
    bool m_ShrinkNarrow;
    Qt::TextElideMode m_Cutdown;
    bool m_MultiLine;
    int  m_Ascent;
    int  m_Descent;
    int  m_leftBearing;
    int  m_rightBearing;
    int  m_Leading;
    int  m_extraLeading;
    int  m_lineHeight;
    int  m_textCursor;

    QVector<QTextLayout *> m_Layouts;

    MythFontProperties* m_Font;
    QMap<QString, MythFontProperties> m_FontStates;

    bool m_colorCycling;
    QColor m_startColor, m_endColor;
    int m_numSteps, m_curStep;
    float curR, curG, curB;
    float incR, incG, incB;

    // Default delay of 3 seconds before 'bouncing' the scrolling text
    enum Constants {ScrollBounceDelay = MythMainWindow::drawRefresh * 3};
    enum ScrollDir {ScrollNone, ScrollLeft, ScrollRight, ScrollUp, ScrollDown,
                    ScrollHorizontal, ScrollVertical};

    int   m_scrollStartDelay;
    int   m_scrollReturnDelay;
    int   m_scrollPause;
    float m_scrollForwardRate;
    float m_scrollReturnRate;
    bool  m_scrollBounce;
    int   m_scrollOffset;
    float m_scrollPos;
    int   m_scrollPosWhole;
    ScrollDir m_scrollDirection;
    bool  m_scrolling;

    enum TextCase {CaseNormal, CaseUpper, CaseLower, CaseCapitaliseFirst,
                   CaseCapitaliseAll};

    TextCase m_textCase;

    friend class MythUITextEdit;
    friend class MythUIButton;
    friend class MythThemedMenu;
    friend class MythThemedMenuPrivate;
};

#endif
