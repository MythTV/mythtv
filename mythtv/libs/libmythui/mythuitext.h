#ifndef MYTHUI_TEXT_H_
#define MYTHUI_TEXT_H_

// QT headers
#include <QColor>

// Mythdb headers
#include "mythstorage.h"

// Mythui headers
#include "mythuitype.h"

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
class MPUBLIC MythUIText : public MythUIType, public StorageUser
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

    void UseAlternateArea(bool useAlt);

    virtual void Pulse(void);

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
    void SetCutDown(bool cut);
    bool GetCutDown(void) const { return m_Cutdown; }
    void SetMultiLine(bool multiline);
    bool GetMultiLine(void) const { return m_MultiLine; }

    void SetArea(const MythRect &rect);
    void SetPosition(const MythPoint &pos);
    MythRect GetDrawRect(void) { return m_drawRect; }

    void SetDrawRectSize(const int width, const int height);
    void SetDrawRectPosition(const int x, const int y);
    void MoveDrawRect(const int x, const int y);

    bool MakeNarrow(QRect &min_rect);
    void FillCutMessage(void);
    QString cutDown(const QString &data, MythFontProperties *font,
                    bool multiline = false);

    int m_Justification;
    MythRect m_OrigDisplayRect;
    MythRect m_AltDisplayRect;
    MythRect m_drawRect;

    QString m_Message;
    QString m_CutMessage;
    QString m_DefaultMessage;
    QString m_TemplateText;

    bool m_Cutdown;
    bool m_MultiLine;

    MythFontProperties* m_Font;
    QMap<QString, MythFontProperties> m_FontStates;

    bool m_colorCycling;
    QColor m_startColor, m_endColor;
    int m_numSteps, m_curStep;
    float curR, curG, curB;
    float incR, incG, incB;

    enum ScrollDir {ScrollLeft, ScrollRight, ScrollUp, ScrollDown};

    bool m_scrolling;
    ScrollDir m_scrollDirection;

    enum TextCase {CaseNormal, CaseUpper, CaseLower, CaseCapitaliseFirst,
                   CaseCapitaliseAll};

    TextCase m_textCase;

    friend class MythUITextEdit;
    friend class MythUIButton;
    friend class MythThemedMenu;
    friend class MythThemedMenuPrivate;
};

#endif
