#ifndef MYTHUIANIMATION_H
#define MYTHUIANIMATION_H

#include "xmlparsebase.h"
#include <QVariantAnimation>

class MythUIType;

class UIEffects
{
  public:
    enum Centre { TopLeft, Top, TopRight,
                  Left, Middle, Right,
                  BottomLeft, Bottom, BottomRight };

    UIEffects()
      : alpha(255), hzoom(1.0), vzoom(1.0), angle(0.0), centre(Middle) { }

    QPointF GetCentre(const QRect &rect, int xoff, int yoff)
    {
        float x = xoff + rect.left();
        float y = yoff + rect.top();
        if (Middle == centre || Top == centre || Bottom == centre)
            x += rect.width() / 2.0;
        if (Middle == centre || Left == centre || Right == centre)
            y += rect.height() / 2.0;
        if (Right == centre || TopRight == centre || BottomRight == centre)
            x += rect.width();
        if (Bottom == centre || BottomLeft == centre || BottomRight == centre)
            y += rect.height();
        return QPointF(x, y);
    }

    int    alpha;
    float  hzoom;
    float  vzoom;
    float  angle;
    Centre centre;
};

class MythUIAnimation : public QVariantAnimation, XMLParseBase
{
  public:
    enum Type    { Alpha, Position, Zoom, HorizontalZoom, VerticalZoom, Angle };
    enum Trigger { AboutToHide, AboutToShow };

    MythUIAnimation(MythUIType* parent = NULL,
                    Trigger trigger = AboutToShow, Type type = Alpha);
    void Activate(void);
    void CopyFrom(const MythUIAnimation* animation);
    Trigger GetTrigger(void) const { return m_trigger; }
    QVariant Value() const { return m_value; }
    bool IsActive() const { return m_active; }

    virtual void updateCurrentValue(const QVariant& value);

    void IncrementCurrentTime(void);
    void SetEasingCurve(const QString &curve);
    void SetCentre(const QString &centre);
    void SetLooped(bool looped)  { m_looped = looped;  }
    void SetReversible(bool rev) { m_reversible = rev; }

    static void ParseElement(const QDomElement& element, MythUIType* parent);

  private:
    static void ParseSection(const QDomElement &element,
                             MythUIType* parent, Trigger trigger);
    static void parseAlpha(const QDomElement& element, QVariant& startValue,
                           QVariant& endValue);
    static void parsePosition(const QDomElement& element, QVariant& startValue,
                              QVariant& endValue, MythUIType *parent);
    static void parseZoom(const QDomElement& element, QVariant& startValue,
                          QVariant& endValue);
    static void parseAngle(const QDomElement& element, QVariant& startValue,
                           QVariant& endValue);

    MythUIType* m_parent;
    Type        m_type;
    Trigger     m_trigger;
    UIEffects::Centre m_centre;
    QVariant    m_value;
    bool        m_active;
    bool        m_looped;
    bool        m_reversible;
};

#endif // MYTHUIANIMATION_H
