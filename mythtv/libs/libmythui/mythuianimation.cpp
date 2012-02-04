#include "mythuianimation.h"
#include "mythuitype.h"
#include "mythmainwindow.h"

#include <QDomDocument>

MythUIAnimation::MythUIAnimation(MythUIType* parent, Trigger trigger, Type type)
    : m_parent(parent), m_type(type), m_trigger(trigger),
      m_centre(UIEffects::Middle), m_active(false), m_looped(false),
      m_reversible(false)
{
}

void MythUIAnimation::Activate(void)
{
    m_active = true;
    setCurrentTime(0);
}

void MythUIAnimation::updateCurrentValue(const QVariant& value)
{
    if (!m_active)
        return;

    m_value = value;
    if (m_parent)
    {
        m_parent->SetCentre(m_centre);

        if (Position == m_type)
            m_parent->SetPosition(m_value.toPoint());
        else if (Alpha == m_type)
            m_parent->SetAlpha(m_value.toInt());
        else if (Zoom == m_type)
            m_parent->SetZoom(m_value.toFloat());
        else if (HorizontalZoom == m_type)
            m_parent->SetHorizontalZoom(m_value.toFloat());
        else if (VerticalZoom == m_type)
            m_parent->SetVerticalZoom(m_value.toFloat());
        else if (Angle == m_type)
            m_parent->SetAngle(m_value.toFloat());
    }
}

void MythUIAnimation::CopyFrom(const MythUIAnimation* animation)
{
    m_type = animation->m_type;
    m_value = animation->m_value;
    m_trigger = animation->m_trigger;
    m_looped = animation->m_looped;
    m_reversible = animation->m_reversible;
    m_centre = animation->m_centre;

    setStartValue(animation->startValue());
    setEndValue(animation->endValue());
    setEasingCurve(animation->easingCurve());
    setDuration(animation->duration());
    if (m_looped)
        setLoopCount(-1);
}

void MythUIAnimation::IncrementCurrentTime(void)
{
    if (!m_active)
        return;

    int time = currentTime();
    if (direction() == Forward)
        time += GetMythMainWindow()->GetDrawInterval();
    else
        time -= GetMythMainWindow()->GetDrawInterval();

    setCurrentTime(time);

    if (endValue() == currentValue())
    {
        if (direction() == Forward)
        {
            if (m_reversible)
                setDirection(Backward);
            else if (!m_looped)
                m_active = false;
        }
    }
    else if (startValue() == currentValue())
    {
        if (direction() == Backward)
        {
            if (m_reversible)
                setDirection(Forward);
            else if (!m_looped)
                m_active = false;
        }
    }
}

void MythUIAnimation::SetEasingCurve(const QString& curve)
{
    if (curve      == "Linear")       setEasingCurve(QEasingCurve::Linear);
    else if (curve == "InQuad")       setEasingCurve(QEasingCurve::InQuad);
    else if (curve == "OutQuad")      setEasingCurve(QEasingCurve::OutQuad);
    else if (curve == "InOutQuad")    setEasingCurve(QEasingCurve::InOutQuad);
    else if (curve == "OutInQuad")    setEasingCurve(QEasingCurve::OutInQuad);
    else if (curve == "InCubic")      setEasingCurve(QEasingCurve::InCubic);
    else if (curve == "OutCubic")     setEasingCurve(QEasingCurve::OutCubic);
    else if (curve == "InOutCubic")   setEasingCurve(QEasingCurve::InOutCubic);
    else if (curve == "OutInCubic")   setEasingCurve(QEasingCurve::OutInCubic);
    else if (curve == "InQuart")      setEasingCurve(QEasingCurve::InQuart);
    else if (curve == "OutQuart")     setEasingCurve(QEasingCurve::OutQuart);
    else if (curve == "InOutQuart")   setEasingCurve(QEasingCurve::InOutQuart);
    else if (curve == "OutInQuart")   setEasingCurve(QEasingCurve::OutInQuart);
    else if (curve == "InQuint")      setEasingCurve(QEasingCurve::InQuint);
    else if (curve == "OutQuint")     setEasingCurve(QEasingCurve::OutQuint);
    else if (curve == "InOutQuint")   setEasingCurve(QEasingCurve::InOutQuint);
    else if (curve == "OutInQuint")   setEasingCurve(QEasingCurve::OutInQuint);
    else if (curve == "InSine")       setEasingCurve(QEasingCurve::InSine);
    else if (curve == "OutSine")      setEasingCurve(QEasingCurve::OutSine);
    else if (curve == "InOutSine")    setEasingCurve(QEasingCurve::InOutSine);
    else if (curve == "OutInSine")    setEasingCurve(QEasingCurve::OutInSine);
    else if (curve == "InExpo")       setEasingCurve(QEasingCurve::InExpo);
    else if (curve == "OutExpo")      setEasingCurve(QEasingCurve::OutExpo);
    else if (curve == "InOutExpo")    setEasingCurve(QEasingCurve::InOutExpo);
    else if (curve == "OutInExpo")    setEasingCurve(QEasingCurve::OutInExpo);
    else if (curve == "InCirc")       setEasingCurve(QEasingCurve::InCirc);
    else if (curve == "OutCirc")      setEasingCurve(QEasingCurve::OutCirc);
    else if (curve == "InOutCirc")    setEasingCurve(QEasingCurve::InOutCirc);
    else if (curve == "OutInCirc")    setEasingCurve(QEasingCurve::OutInCirc);
    else if (curve == "InElastic")    setEasingCurve(QEasingCurve::InElastic);
    else if (curve == "OutElastic")   setEasingCurve(QEasingCurve::OutElastic);
    else if (curve == "InOutElastic") setEasingCurve(QEasingCurve::InOutElastic);
    else if (curve == "OutInElastic") setEasingCurve(QEasingCurve::OutInElastic);
    else if (curve == "InOutBack")    setEasingCurve(QEasingCurve::InOutBack);
    else if (curve == "OutInBack")    setEasingCurve(QEasingCurve::OutInBack);
    else if (curve == "InBounce")     setEasingCurve(QEasingCurve::InBounce);
    else if (curve == "OutBounce")    setEasingCurve(QEasingCurve::OutBounce);
    else if (curve == "InOutBounce")  setEasingCurve(QEasingCurve::InOutBounce);
    else if (curve == "OutInBounce")  setEasingCurve(QEasingCurve::OutInBounce);
    else if (curve == "InCurve")      setEasingCurve(QEasingCurve::InCurve);
    else if (curve == "OutCurve")     setEasingCurve(QEasingCurve::OutCurve);
    else if (curve == "SineCurve")    setEasingCurve(QEasingCurve::SineCurve);
    else if (curve == "CosineCurve")  setEasingCurve(QEasingCurve::CosineCurve);
}

void MythUIAnimation::SetCentre(const QString &centre)
{
    if (centre      == "topleft")     m_centre = UIEffects::TopLeft;
    else if (centre == "top")         m_centre = UIEffects::Top;
    else if (centre == "topright")    m_centre = UIEffects::TopRight;
    else if (centre == "left")        m_centre = UIEffects::Left;
    else if (centre == "middle")      m_centre = UIEffects::Middle;
    else if (centre == "right")       m_centre = UIEffects::Right;
    else if (centre == "bottomleft")  m_centre = UIEffects::BottomLeft;
    else if (centre == "bottom")      m_centre = UIEffects::Bottom;
    else if (centre == "bottomright") m_centre = UIEffects::BottomRight;
}

void MythUIAnimation::ParseElement(const QDomElement &element,
                                   MythUIType* parent)
{
    QString t = element.attribute("trigger", "AboutToShow");
    Trigger trigger = AboutToShow;
    if ("AboutToHide" == t)
        trigger = AboutToHide;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement section = child.toElement();
        if (section.isNull())
            continue;
        if (section.tagName() == "section")
            ParseSection(section, parent, trigger);
    }
}

void MythUIAnimation::ParseSection(const QDomElement &element,
                                   MythUIType* parent, Trigger trigger)
{
    int duration = element.attribute("duration", "500").toInt();
    QString centre = element.attribute("centre", "Middle");
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement effect = child.toElement();
        if (effect.isNull())
            continue;

        Type type = Alpha;
        int effectduration = duration;
        // override individual durations
        QString effect_duration = effect.attribute("duration", "");
        if (!effect_duration.isEmpty())
            effectduration = effect_duration.toInt();

        bool looped = parseBool(effect.attribute("looped", "false"));
        bool reversible = parseBool(effect.attribute("reversible", "false"));
        QString easingcurve = effect.attribute("easingcurve", "Linear");
        QVariant start;
        QVariant end;

        QString fxtype = effect.tagName();
        if (fxtype == "alpha")
        {
            type = Alpha;
            parseAlpha(effect, start, end);
        }
        else if (fxtype == "position")
        {
            type = Position;
            parsePosition(effect, start, end, parent);
        }
        else if (fxtype == "angle")
        {
            type = Angle;
            parseAngle(effect, start, end);
        }
        else if (fxtype == "zoom")
        {
            type = Zoom;
            parseZoom(effect, start, end);
        }
        else if (fxtype == "horizontalzoom")
        {
            type = HorizontalZoom;
            parseZoom(effect, start, end);
        }
        else if (fxtype == "verticalzoom")
        {
            type = VerticalZoom;
            parseZoom(effect, start, end);
        }
        else
            continue;

        MythUIAnimation* a = new MythUIAnimation(parent, trigger, type);
        a->setStartValue(start);
        a->setEndValue(end);
        a->setDuration(effectduration);
        a->SetEasingCurve(easingcurve);
        a->SetCentre(centre);
        a->SetLooped(looped);
        a->SetReversible(reversible);
        if (looped)
            a->setLoopCount(-1);
        parent->GetAnimations()->append(a);
    }
}

void MythUIAnimation::parseAlpha(const QDomElement& element,
                                 QVariant& startValue, QVariant& endValue)
{
    startValue = element.attribute("start", "0").toInt();
    endValue = element.attribute("end", "0").toInt();
}

void MythUIAnimation::parsePosition(const QDomElement& element,
                                    QVariant& startValue, QVariant& endValue,
                                    MythUIType *parent)
{
    MythPoint start = parsePoint(element.attribute("start", "0,0"), false);
    MythPoint startN = parsePoint(element.attribute("start", "0,0"));
    MythPoint end = parsePoint(element.attribute("end", "0,0"), false);
    MythPoint endN = parsePoint(element.attribute("end", "0,0"));

    if (start.x() == -1)
        startN.setX(parent->GetArea().x());

    if (start.y() == -1)
        startN.setY(parent->GetArea().y());

    if (end.x() == -1)
        endN.setX(parent->GetArea().x());

    if (end.y() == -1)
        endN.setY(parent->GetArea().y());

    startN.CalculatePoint(parent->GetArea());
    endN.CalculatePoint(parent->GetArea());

    startValue = startN.toQPoint();
    endValue = endN.toQPoint();
}

void MythUIAnimation::parseZoom(const QDomElement& element,
                                QVariant& startValue, QVariant& endValue)
{
    startValue = element.attribute("start", "0").toFloat() / 100.0;
    endValue = element.attribute("end", "0").toFloat() /100.0;
}

void MythUIAnimation::parseAngle(const QDomElement& element,
                                 QVariant& startValue, QVariant& endValue)
{
    startValue = element.attribute("start", "0").toFloat();
    endValue = element.attribute("end", "0").toFloat();
}
