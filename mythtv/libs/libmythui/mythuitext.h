#ifndef MYTHUI_TEXT_H_
#define MYTHUI_TEXT_H_

#include "mythuitype.h"
#include "mythstorage.h"

#include <QImage>
#include <QColor>

#include "mythuitype.h"

class MythFontProperties;

class MythUIText : public MythUIType, public StorageUser
{
  public:
    MythUIText(MythUIType *parent, const QString &name);
    MythUIText(const QString &text, const MythFontProperties &font,
               QRect displayRect, QRect altDisplayRect,
               MythUIType *parent, const QString &name);
    ~MythUIText();

    void Reset(void);

    void SetText(const QString &text);
    QString GetText(void) const;
    QString GetDefaultText(void) const;

    void SetFontProperties(const MythFontProperties &fontProps);
    const MythFontProperties* GetFontProperties() { return m_Font; }

    void UseAlternateArea(bool useAlt);

    void SetJustification(int just);
    int GetJustification(void);
    void SetCutDown(bool cut);

    void SetArea(const MythRect &rect);
    void SetPosition(const MythPoint &pos);
    MythRect GetDrawRect(void) { return m_drawRect; }
    void SetStartPosition(const int x, const int y);
    void MoveStartPosition(const int x, const int y);

    virtual void Pulse(void);

    void CycleColor(QColor startColor, QColor endColor, int numSteps);
    void StopCycling();

    // StorageUser
    void SetDBValue(const QString &text) { SetText(text); }
    QString GetDBValue(void) const { return GetText(); }

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    int m_Justification;
    MythRect m_OrigDisplayRect;
    MythRect m_AltDisplayRect;
    MythRect m_drawRect;

    QString m_Message;
    QString m_CutMessage;
    QString m_DefaultMessage;

    bool m_Cutdown;

    MythFontProperties* m_Font;

    bool m_colorCycling;
    QColor m_startColor, m_endColor;
    int m_numSteps, m_curStep;
    float curR, curG, curB;
    float incR, incG, incB;
};

#endif
