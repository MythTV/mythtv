
// Own header
#include "mythuitype.h"

// C++ headers
#include <algorithm>
#include <utility>

// QT headers
#include <QDomDocument>
#include <QEvent>
#include <QKeyEvent>
#include <QInputMethodEvent>

// XML headers
#include "xmlparsebase.h"

// Mythbase headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmedia.h"
#include "libmythbase/mythrandom.h"
#ifdef _MSC_VER
#  include "libmythbase/compat.h"   // random
#endif

// MythUI headers
#include "mythgesture.h"
#include "mythimage.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuitext.h"
#include "mythuiimage.h"
#include "mythuibutton.h"
#include "mythuicheckbox.h"
#include "mythuibuttonlist.h"
#include "mythuitextedit.h"
#include "mythuiprogressbar.h"
#include "mythuispinbox.h"
#include "mythuigroup.h"

#define LOC      QString("MythUIType: ")

MythUIType::MythUIType(QObject *parent, const QString &name)
    : QObject(parent)
{
    setObjectName(name);

    if (parent)
    {
        m_parent = qobject_cast<MythUIType *>(parent);

        if (m_parent)
            m_parent->AddChild(this);
    }

    m_fonts = new FontMap();

    // for debugging/theming
    m_borderColor = QColor(MythRandom(0, 255), MythRandom(0, 255), MythRandom(0, 255));
}

MythUIType::~MythUIType()
{
    delete m_fonts;
    qDeleteAll(m_animations);
}

/**
 *  \brief Reset the widget to it's original state, should not reset changes
 *         made by the theme
 */
void MythUIType::Reset()
{
    // Reset all children
    QMutableListIterator<MythUIType *> it(m_childrenList);

    while (it.hasNext())
    {
        it.next();
        MythUIType *type = it.value();
        type->Reset();
    }
}

/**
 *  \brief Add a child UIType
 */
void MythUIType::AddChild(MythUIType *child)
{
    if (!child)
        return;

    m_childrenList.push_back(child);
}

static QObject *qChildHelper(const char *objName, const char *inheritsClass,
                             bool recursiveSearch, const QObjectList &children)
{
    if (children.isEmpty())
        return nullptr;

    bool onlyWidgets = (inheritsClass
                        && qstrcmp(inheritsClass, "QWidget") == 0);
    const QLatin1String oName(objName);

    for (auto *obj : std::as_const(children))
    {
        if (onlyWidgets)
        {
            if (obj->isWidgetType() && (!objName || obj->objectName() == oName))
                return obj;
        }
        else if ((!inheritsClass || obj->inherits(inheritsClass))
                 && (!objName || obj->objectName() == oName))
        {
            return obj;
        }

        if (recursiveSearch && (qobject_cast<MythUIGroup *>(obj) != nullptr))
        {
            obj = qChildHelper(objName, inheritsClass, recursiveSearch,
                               obj->children());
            if (obj != nullptr)
                return obj;
        }
    }

    return nullptr;
}

/**
 *  \brief Get a named child of this UIType
 *
 *  \param name Name of child
 *  \return Pointer to child if found, or nullptr
 */
MythUIType *MythUIType::GetChild(const QString &name) const
{
    QObject *ret = qChildHelper(name.toLatin1().constData(), nullptr, true, children());

    if (ret)
        return qobject_cast<MythUIType *>(ret);

    return nullptr;
}

/**
 *  \brief Delete a named child of this UIType
 *
 *  \param name Name of child
 */
void MythUIType::DeleteChild(const QString &name)
{
    QMutableListIterator<MythUIType *> it(m_childrenList);

    while (it.hasNext())
    {
        it.next();
        MythUIType *type = it.value();

        if (type->objectName() == name)
        {
            delete type;
            it.remove();
            return;
        }
    }
}

/**
 *  \brief Delete the given UIType if it is a child of this UIType.
 *
 *  Will not delete the object if it is not a child. Pointer will be set to
 *  nullptr if successful.
 */
void MythUIType::DeleteChild(MythUIType *child)
{
    if (!child)
        return;

    QMutableListIterator<MythUIType *> it(m_childrenList);

    while (it.hasNext())
    {
        it.next();
        MythUIType *type = it.value();

        if (type == child)
        {
            delete type;
            it.remove();
            child = nullptr;
            return;
        }
    }
}

/**
 *  \brief Return a list of all child widgets
 */
QList<MythUIType *> *MythUIType::GetAllChildren(void)
{
    return &m_childrenList;
}

QList<MythUIType *> MythUIType::GetAllDescendants(void)
{
    QList<MythUIType *> descendants {};

    for (const auto & item :std::as_const(m_childrenList))
    {
        descendants += item;
        descendants += item->GetAllDescendants();
    }
    return descendants;
}

/**
 *  \brief Delete all child widgets
 */
void MythUIType::DeleteAllChildren(void)
{
    QList<MythUIType *>::iterator it;

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
        if (*it)
            delete *it;

    m_childrenList.clear();
}

/** \brief Return the first MythUIType at the given coordinates
 *
 *  \param p QPoint coordinates
 *  \param recursive Whether to perform a recursive search
 *  \param focusable Only consider widgets that are focusable.
 *
 *  \return The widget at these coordinates
 */
MythUIType *MythUIType::GetChildAt(const QPoint p, bool recursive,
                                   bool focusable) const
{
    if (GetArea().contains(p))
    {
        if (!IsVisible() || !IsEnabled())
            return nullptr;

        if (m_childrenList.isEmpty())
            return nullptr;

        /* check all children */
        QList<MythUIType *>::const_reverse_iterator it;

        for (it = m_childrenList.rbegin(); it != m_childrenList.rend(); it++)
        {
            if (!(*it))
                continue;

            // If this point doesn't fall within the child's area then move on
            // This requires that the area is actually accurate and in some
            // cases this still isn't true
            if (!(*it)->GetArea().contains(p - GetArea().topLeft()))
                continue;


            MythUIType *child = *it;

            if (recursive && (focusable && !child->CanTakeFocus()))
                child = child->GetChildAt(p - GetArea().topLeft(), recursive,
                                          focusable);

            if (child)
            {
                // NOTE: Assumes no selectible ui type will contain another
                // selectible ui type.
                if (focusable && !child->CanTakeFocus())
                    continue;

                return child;
            }
        }
    }

    return nullptr;
}

void MythUIType::ActivateAnimations(MythUIAnimation::Trigger trigger)
{
    for (MythUIAnimation* animation : std::as_const(m_animations))
        if (animation->GetTrigger() == trigger)
            animation->Activate();

    for (MythUIType* uiType : std::as_const(m_childrenList))
        uiType->ActivateAnimations(trigger);
}

bool MythUIType::NeedsRedraw(void) const
{
    return m_needsRedraw;
}

void MythUIType::ResetNeedsRedraw(void)
{
    m_needsRedraw = false;

    QList<MythUIType *>::Iterator it;

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
        (*it)->ResetNeedsRedraw();
}

void MythUIType::SetRedraw(void)
{
    if (m_area.width() == 0 || m_area.height() == 0)
        return;

    m_needsRedraw = true;

    if (m_dirtyRegion.isEmpty())
        m_dirtyRegion = QRegion(m_area.toQRect());
    else
        m_dirtyRegion = m_dirtyRegion.united(QRegion(m_area.toQRect()));

    if (m_parent)
        m_parent->SetChildNeedsRedraw(this);
}

void MythUIType::SetChildNeedsRedraw(MythUIType *child)
{
    QRegion childRegion = child->GetDirtyArea();

    if (childRegion.isEmpty())
        return;

    childRegion.translate(m_area.x(), m_area.y());

    childRegion = childRegion.intersected(m_area.toQRect());

    m_needsRedraw = true;

    if (m_dirtyRegion.isEmpty())
        m_dirtyRegion = childRegion;
    else
        m_dirtyRegion = m_dirtyRegion.united(childRegion);

    if (m_parent)
        m_parent->SetChildNeedsRedraw(this);
}

/**
 *  \brief Return if this widget can accept input focus
 */
bool MythUIType::CanTakeFocus(void) const
{
    return m_canHaveFocus;
}

/**
 *  \brief Set whether this widget can take focus
 */
void MythUIType::SetCanTakeFocus(bool set)
{
    m_canHaveFocus = set;
}

/**
 *  \brief Handle one frame of a movement animation.
 *
 * This changes the position of the widget within it's parent's area
 */
void MythUIType::HandleMovementPulse(void)
{
    if (!GetPainter()->SupportsAnimation())
        return;

    if (!m_moving)
        return;

    QPoint curXY = m_area.topLeft().toQPoint();
    m_dirtyRegion = m_area.toQRect();

    int xdir = m_xyDestination.x() - curXY.x();
    int ydir = m_xyDestination.y() - curXY.y();

    curXY.setX(curXY.x() + m_xySpeed.x());
    curXY.setY(curXY.y() + m_xySpeed.y());

    if ((xdir > 0 && curXY.x() >= m_xyDestination.x()) ||
        (xdir < 0 && curXY.x() <= m_xyDestination.x()) ||
        (xdir == 0))
    {
        m_xySpeed.setX(0);
    }

    if ((ydir > 0 && curXY.y() >= m_xyDestination.y()) ||
        (ydir <= 0 && curXY.y() <= m_xyDestination.y()) ||
        (ydir == 0))
    {
        m_xySpeed.setY(0);
    }

    SetRedraw();

    if (m_xySpeed.x() == 0 && m_xySpeed.y() == 0)
    {
        m_moving = false;
        emit FinishedMoving();
    }

    m_area.moveTopLeft(curXY);
}

/**
 *  \brief Handle one frame of an alpha (transparency) change animation.
 *
 * This changes the alpha value of the widget
 */
void MythUIType::HandleAlphaPulse(void)
{
    if (!GetPainter()->SupportsAlpha() ||
        !GetPainter()->SupportsAnimation())
        return;

    if (m_alphaChangeMode == 0)
        return;

    m_effects.m_alpha += m_alphaChange;

    m_effects.m_alpha = std::clamp(m_effects.m_alpha, m_alphaMin, m_alphaMax);

    // Reached limits so change direction
    if (m_effects.m_alpha == m_alphaMax || m_effects.m_alpha == m_alphaMin)
    {
        if (m_alphaChangeMode == 2)
        {
            m_alphaChange *= -1;
        }
        else
        {
            m_alphaChangeMode = 0;
            m_alphaChange = 0;
            emit FinishedFading();
        }
    }

    SetRedraw();
}

/**
 *  \brief Pulse is called 70 times a second to trigger a single frame of an
 *         animation
 *
 * This changes the alpha value of the widget
 */
void MythUIType::Pulse(void)
{
    if (!m_visible || m_vanished)
        return;

    HandleMovementPulse();
    HandleAlphaPulse();

    QList<MythUIAnimation*>::Iterator i;
    for (i = m_animations.begin(); i != m_animations.end(); ++i)
        (*i)->IncrementCurrentTime();

    QList<MythUIType *>::Iterator it;

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
        (*it)->Pulse();
}

int MythUIType::CalcAlpha(int alphamod) const
{
    return (int)(m_effects.m_alpha * (alphamod / 255.0));
}

void MythUIType::DrawSelf(MythPainter * /*p*/, int /*xoffset*/, int /*yoffset*/,
                          int /*alphaMod*/, QRect /*clipRect*/)
{
}

void MythUIType::Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod,
                      QRect clipRect)
{
    // NB m_dirtyRegion may be extended by HandleMovementPulse, SetRedraw
    // or SetChildNeedsRedraw etc _AFTER_ GetDirtyArea is called.
    // So clipRect may not include the whole of m_dirtyRegion
    m_dirtyRegion -= QRegion(clipRect); // NB Qt >= 4.2

    if (!m_visible || m_vanished)
        return;

    QRect realArea = m_area.toQRect();
    realArea.translate(xoffset, yoffset);

    if (!realArea.intersects(clipRect))
        return;

    p->PushTransformation(m_effects, m_effects.GetCentre(m_area, xoffset, yoffset));

    DrawSelf(p, xoffset, yoffset, alphaMod, clipRect);

    QList<MythUIType *>::Iterator it;

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
    {
        (*it)->Draw(p, xoffset + m_area.x(), yoffset + m_area.y(),
                    CalcAlpha(alphaMod), clipRect);
    }

    if (p->ShowBorders())
    {
        static const QBrush kNullBrush(Qt::NoBrush);
        QPen pen(m_borderColor);
        pen.setWidth(1);
        p->DrawRect(realArea, kNullBrush, pen, 255);

        if (p->ShowTypeNames())
        {
            MythFontProperties font;
            font.SetFace(QFont("Droid Sans"));
            font.SetColor(m_borderColor);
            font.SetPointSize(8);
            p->DrawText(realArea, objectName(), 0, font, 255, realArea);
        }
    }

    p->PopTransformation();
}

void MythUIType::SetPosition(int x, int y)
{
    SetPosition(MythPoint(x, y));
}

void MythUIType::SetPosition(QPoint point)
{
    SetPosition(MythPoint(point));
}

void MythUIType::SetPosition(const MythPoint &point)
{
    MythPoint  pos(point);

    if (m_parent)
        pos.CalculatePoint(m_parent->GetFullArea());
    else
        pos.CalculatePoint(GetMythMainWindow()->GetUIScreenRect());

    if (m_area.topLeft() == pos)
        return;

    m_dirtyRegion = QRegion(m_area.toQRect());

    m_area.moveTopLeft(pos);

    RecalculateArea(false);

    SetRedraw();
}

MythPoint MythUIType::GetPosition(void) const
{
    return m_area.topLeft();
}

void MythUIType::SetSize(const QSize size)
{
    if (size == m_area.size())
        return;

    m_dirtyRegion = QRegion(m_area.toQRect());

    m_area.setSize(size);
    RecalculateArea();

    if (m_parent)
        m_parent->ExpandArea(m_area.toQRect());

    SetRedraw();
}

/**
 * \brief Set the minimum size of this widget, for widgets which can be rescaled
 *
 * Use MythPoint to represent size, so percentages can be used
 */
void MythUIType::SetMinSize(const MythPoint &minsize)
{
    MythPoint point(minsize);

    if (m_parent)
        point.CalculatePoint(m_parent->GetFullArea());

    m_minSize = point;

    SetRedraw();
}

QSize MythUIType::GetMinSize(void) const
{
    if (!m_minSize.isValid())
        return m_area.size();

    return {m_minSize.x(), m_minSize.y()};
}

void MythUIType::SetArea(const MythRect &rect)
{
    if (rect == m_area)
        return;

    m_dirtyRegion = QRegion(m_area.toQRect());

    m_area = rect;
    RecalculateArea();

    if (m_parent)
        m_parent->ExpandArea(m_area.toQRect());

    SetRedraw();
}

/**
 * Adjust the size of a sibling.
 */
void MythUIType::AdjustMinArea(int delta_x, int delta_y,
                               int delta_w, int delta_h)
{
    // If a minsize is not set, don't use MinArea
    if (!m_minSize.isValid())
        return;

    // Delta's are negative values; knock down the area
    QRect bounded(m_area.x() - delta_x,
                  m_area.y() - delta_y,
                  m_area.width() + delta_w,
                  m_area.height() + delta_h);

    // Make sure we have not violated the min size
    if (!bounded.isNull() || !m_vanish)
    {
        QPoint center = bounded.center();

        if (bounded.isNull())
            bounded.setSize(GetMinSize());
        else
            bounded.setSize(bounded.size().expandedTo(GetMinSize()));

        bounded.moveCenter(center);
    }

    if (bounded.x() + bounded.width() > m_area.x() + m_area.width())
        bounded.moveRight(m_area.x() + m_area.width());
    if (bounded.y() + bounded.height() > m_area.y() + m_area.height())
        bounded.moveBottom(m_area.y() + m_area.height());
    if (bounded.x() < m_area.x())
    {
        bounded.moveLeft(m_area.x());
        if (bounded.width() > m_area.width())
            bounded.setWidth(m_area.width());
    }
    if (bounded.y() < m_area.y())
    {
        bounded.moveTop(m_area.y());
        if (bounded.height() > m_area.height())
            bounded.setHeight(m_area.height());
    }

    m_minArea = bounded;
    m_vanished = false;

    QList<MythUIType *>::iterator it;

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
    {
        if (!(*it)->m_initiator)
            (*it)->AdjustMinArea(delta_x, delta_y, delta_w, delta_h);
    }
}

void MythUIType::VanishSibling(void)
{
    if (!m_minSize.isValid() || !m_vanish)
        return;

    m_minArea.moveLeft(0);
    m_minArea.moveTop(0);
    m_minArea.setWidth(0);
    m_minArea.setHeight(0);
    m_vanished = true;

    QList<MythUIType *>::iterator it;

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
    {
        if (!(*it)->m_initiator)
            (*it)->VanishSibling();
    }
}

/**
 * Adjust the size of sibling objects within the button.
 */
void MythUIType::SetMinAreaParent(MythRect actual_area, MythRect allowed_area,
                                  MythUIType *calling_child)
{
    int delta_x = 0;
    int delta_y = 0;
    int delta_w = 0;
    int delta_h = 0;
    MythRect area;

    // If a minsize is not set, don't use MinArea
    if (!m_minSize.isValid())
        return;

    if (calling_child->m_vanished)
    {
        actual_area.moveLeft(0);
        actual_area.moveTop(0);
        allowed_area.moveLeft(0);
        allowed_area.moveTop(0);
    }

    actual_area.translate(m_area.x(), m_area.y());
    allowed_area.translate(m_area.x(), m_area.y());

    QList<MythUIType *>::iterator it;

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
    {
        if (*it == calling_child || !(*it)->m_initiator)
            continue;

        if (!(*it)->m_vanished)
        {
            // Find union of area(s)
            area = (*it)->GetArea();
            area.translate(m_area.x(), m_area.y());
            actual_area = actual_area.united(area);

            area = (*it)->m_area;
            area.translate(m_area.x(), m_area.y());
            allowed_area = allowed_area.united(area);
        }
    }

    // Make sure it is not larger than the area allowed
    actual_area = actual_area.intersected(m_area);
    allowed_area = allowed_area.intersected(m_area);

    if (m_vanish && actual_area.size().isNull())
    {
        m_vanished = true;
    }
    else
    {
        if (calling_child->m_vanished)
        {
            delta_x = m_area.x() - actual_area.x();
            delta_y = m_area.y() - actual_area.y();
            delta_w = actual_area.width() - m_area.width();
            delta_h = actual_area.height() - m_area.height();
        }
        else
        {
            delta_x = allowed_area.x() - actual_area.x();
            delta_y = allowed_area.y() - actual_area.y();
            delta_w = actual_area.width() - allowed_area.width();
            delta_h = actual_area.height() - allowed_area.height();
        }

        m_vanished = false;
    }

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
    {
        if (*it == calling_child)
            continue;

        if (!(*it)->m_initiator)
        {
            if (m_vanished)
                (*it)->VanishSibling();
            else
                (*it)->AdjustMinArea(delta_x, delta_y, delta_w, delta_h);
        }

        area = (*it)->GetArea();
        area.translate(m_area.topLeft());
        actual_area = actual_area.united(area);
    }

    if (m_vanished)
    {
        m_minArea.setRect(0, 0, 0, 0);
        actual_area.setRect(0, 0, 0, 0);
    }
    else
    {
        QSize bound(actual_area.width(), actual_area.height());

        bound = bound.expandedTo(GetMinSize());
        m_minArea.setRect(actual_area.x(),
                          actual_area.y(),
                          actual_area.x() + bound.width(),
                          actual_area.y() + bound.height());
    }

    if (m_parent)
        m_parent->SetMinAreaParent(actual_area, m_area, this);
}

/**
 * Set the minimum area based on the given size
 */
void MythUIType::SetMinArea(const MythRect &rect)
{
    // If a minsize is not set, don't use MinArea
    if (!m_initiator || !m_minSize.isValid())
        return;

    QRect bounded(rect);
    bool  vanish = (m_vanish && rect.isNull());

    if (vanish)
    {
        bounded.moveLeft(0);
        bounded.moveTop(0);
    }
    else
    {
        QPoint center = bounded.center();

        if (bounded.isNull())
            bounded.setSize(GetMinSize());
        else
            bounded.setSize(bounded.size().expandedTo(GetMinSize()));

        bounded.moveCenter(center);
        if (bounded.x() + bounded.width() > m_area.x() + m_area.width())
            bounded.moveRight(m_area.x() + m_area.width());
        if (bounded.y() + bounded.height() > m_area.y() + m_area.height())
            bounded.moveBottom(m_area.y() + m_area.height());
        if (bounded.x() < m_area.x())
        {
            bounded.moveLeft(m_area.x());
            if (bounded.width() > m_area.width())
                bounded.setWidth(m_area.width());
        }
        if (bounded.y() < m_area.y())
        {
            bounded.moveTop(m_area.y());
            if (bounded.height() > m_area.height())
                bounded.setHeight(m_area.height());
        }
    }

    m_minArea = bounded;
    m_vanished = vanish;

    if (m_parent)
        m_parent->SetMinAreaParent(m_minArea, m_area, this);
}

void MythUIType::ExpandArea(const QRect rect)
{
    QSize childSize = rect.size();
    QSize size = m_area.size();

    if (childSize == size)
        return;

    SetSize(size.expandedTo(childSize));
    SetRedraw();
}

/**
 * If the object has a minimum area defined, return it, other wise
 * return the default area.
 */
MythRect MythUIType::GetArea(void) const
{
    if (m_vanished || m_minArea.isValid())
        return m_minArea;

    return m_area;
}

MythRect MythUIType::GetFullArea(void) const
{
    return m_area;
}

QRegion MythUIType::GetDirtyArea(void) const
{
    return m_dirtyRegion;
}

bool MythUIType::IsVisible(bool recurse) const
{
    if (recurse)
    {
        if (m_parent && !m_parent->IsVisible(recurse))
            return false;
    }

    return m_visible;
}

void MythUIType::MoveTo(QPoint destXY, QPoint speedXY)
{
    if (!GetPainter()->SupportsAnimation())
        return;

    if (destXY.x() == m_area.x() && destXY.y() == m_area.y())
        return;

    m_moving = true;

    m_xyDestination = destXY;
    m_xySpeed = speedXY;
}

void MythUIType::AdjustAlpha(int mode, int alphachange, int minalpha,
                             int maxalpha)
{
    if (!GetPainter()->SupportsAlpha())
        return;

    m_alphaChangeMode = mode;
    m_alphaChange = alphachange;
    m_alphaMin = minalpha;
    m_alphaMax = maxalpha;

    m_effects.m_alpha = std::clamp(m_effects.m_alpha, m_alphaMin, m_alphaMax);
}

void MythUIType::SetAlpha(int newalpha)
{
    if (m_effects.m_alpha == newalpha)
        return;

    m_effects.m_alpha = newalpha;
    SetRedraw();
}

int MythUIType::GetAlpha(void) const
{
    return m_effects.m_alpha;
}

void MythUIType::SetCentre(UIEffects::Centre centre)
{
    m_effects.m_centre = centre;
}

void MythUIType::SetZoom(float zoom)
{
    SetHorizontalZoom(zoom);
    SetVerticalZoom(zoom);
}

void MythUIType::SetHorizontalZoom(float zoom)
{
    m_effects.m_hzoom = zoom;
    SetRedraw();
}

void MythUIType::SetVerticalZoom(float zoom)
{
    m_effects.m_vzoom = zoom;
    SetRedraw();
}

void MythUIType::SetAngle(float angle)
{
    m_effects.m_angle = angle;
    SetRedraw();
}

/** \brief Key event handler
 *
 *  \param event Keypress event
 */
bool MythUIType::keyPressEvent(QKeyEvent * /*event*/)
{
    return false;
}

/** \brief Input Method event handler
 *
 *  \param event Input Method event
 */
bool MythUIType::inputMethodEvent(QInputMethodEvent * /*event*/)
{
    return false;
}

void MythUIType::customEvent(QEvent *event)
{
    QObject::customEvent(event);
}

/** \brief Mouse click/movement handler, receives mouse gesture events from the
 *         QCoreApplication event loop. Should not be used directly.
 *
 *  \param event Mouse event
 */
bool MythUIType::gestureEvent(MythGestureEvent * /*event*/)
{
    return false;
}

/** \brief Media/Device status event handler, received from MythMediaMonitor
 *
 *  \param event Media event
 */
void MythUIType::mediaEvent(MythMediaEvent * /*event*/)
{
}

void MythUIType::LoseFocus(void)
{
    if (!m_canHaveFocus || !m_hasFocus)
        return;

    emit LosingFocus();
    m_hasFocus = false;
    Refresh();
}

bool MythUIType::TakeFocus(void)
{
    if (!m_canHaveFocus || m_hasFocus)
        return false;

    m_hasFocus = true;
    Refresh();
    emit TakingFocus();
    return true;
}

void MythUIType::SetFocusedName(const QString & widgetname)
{
    m_focusedName = widgetname;
    emit RequestUpdate();
}

void MythUIType::Activate(void)
{
}

void MythUIType::Refresh(void)
{
    SetRedraw();
}

void MythUIType::UpdateDependState(MythUIType *dependee, bool isDefault)
{
    bool visible = false;

    if (dependee)
    {
        bool reverse = m_reverseDepend[dependee];
        visible = reverse ? !isDefault : isDefault;
        // NOLINTNEXTLINE(modernize-loop-convert)
        for (int i = 0; i < m_dependsValue.size(); i++)
        {
            if (m_dependsValue[i].first != dependee)
                continue;
            m_dependsValue[i].second = visible;
            break;
        }
    }

    if (!m_dependsValue.empty())
        visible = m_dependsValue[0].second;
    for (int i = 1; i <  m_dependsValue.size(); i++)
    {
        bool v = m_dependsValue[i].second;

        if (((i-1) < m_dependOperator.size()) &&
            m_dependOperator[i-1] == 1)
        {
            // OR operator
            visible = visible && v;
        }
        else
        {
            // AND operator
            visible = visible || v;
        }
    }

    m_isDependDefault = visible;

    SetVisible(!m_isDependDefault);
}

void MythUIType::UpdateDependState(bool isDefault)
{
    auto *dependee = qobject_cast<MythUIType*>(sender());

    UpdateDependState(dependee, isDefault);
}

void MythUIType::SetVisible(bool visible)
{
    if (visible == m_visible)
        return;

    if (visible && m_isDependDefault)
        return;

    m_visible = visible;
    SetRedraw();

    if (m_visible)
        emit Showing();
    else
        emit Hiding();
    emit VisibilityChanged(m_visible);
}

void MythUIType::SetDependIsDefault(bool isDefault)
{
    m_isDependDefault = isDefault;
}

void MythUIType::SetEnabled(bool enable)
{
    if (m_enabled != enable)
        m_enabled = enable;

    if (enable)
        emit Enabling();
    else
        emit Disabling();
}

void MythUIType::Hide(void)
{
    SetVisible(false);
}

void MythUIType::Show(void)
{
    SetVisible(true);
}

void MythUIType::AddFocusableChildrenToList(FocusInfoType &focusList)
{
    if (m_canHaveFocus)
        focusList.insert(m_focusOrder, this);

    for (auto it = m_childrenList.crbegin(); it != m_childrenList.crend(); ++it)
        (*it)->AddFocusableChildrenToList(focusList);
}

int MythUIType::NormX(const int width)
{
    return GetMythMainWindow()->NormX(width);
}

int MythUIType::NormY(const int height)
{
    return GetMythMainWindow()->NormY(height);
}

/**
 *  \brief Copy this widgets state from another.
 */
void MythUIType::CopyFrom(MythUIType *base)
{
    m_xmlName = base->m_xmlName;
    m_xmlLocation = base->m_xmlLocation;
    m_visible = base->m_visible;
    m_enabled = base->m_enabled;
    m_canHaveFocus = base->m_canHaveFocus;
    m_focusOrder = base->m_focusOrder;

    m_area = base->m_area;
    RecalculateArea();

    m_enableInitiator = base->m_enableInitiator;
    m_minSize = base->m_minSize;
    m_vanish = base->m_vanish;
    m_vanished = false;
    m_effects = base->m_effects;
    m_alphaChangeMode = base->m_alphaChangeMode;
    m_alphaChange = base->m_alphaChange;
    m_alphaMin = base->m_alphaMin;
    m_alphaMax = base->m_alphaMax;

    m_moving = base->m_moving;
    m_xyDestination = base->m_xyDestination;
    m_xySpeed = base->m_xySpeed;
    m_deferload = base->m_deferload;

    QList<MythUIAnimation*>::Iterator i;
    for (i = base->m_animations.begin(); i != base->m_animations.end(); ++i)
    {
        auto* animation = new MythUIAnimation(this);
        animation->CopyFrom(*i);
        m_animations.push_back(animation);
    }

    QList<MythUIType *>::Iterator it;

    for (it = base->m_childrenList.begin(); it != base->m_childrenList.end();
         ++it)
    {
        MythUIType *child = GetChild((*it)->objectName());

        if (child)
            child->CopyFrom(*it);
        else
            (*it)->CreateCopy(this);
    }

    m_dependsMap = base->m_dependsMap;

    SetMinArea(base->m_minArea);
}

/**
 *  \brief Copy the state of this widget to the one given, it must be of the
 *         same type.
 */
void MythUIType::CreateCopy(MythUIType * /*parent*/)
{
    // Calling CreateCopy on base type is not valid
}

/**
 *  \brief Parse the xml definition of this widget setting the state of the
 *         object accordingly
 */
bool MythUIType::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    //FIXME add movement etc.

    if (element.tagName() == "position")
        SetPosition(parsePoint(element));
    else if (element.tagName() == "area")
    {
        SetArea(parseRect(element));
    }
    else if (element.tagName() == "minsize")
    {
        // Use parsePoint so percentages can be used
        if (element.hasAttribute("initiator"))
            m_enableInitiator = parseBool(element.attribute("initiator"));

        if (element.hasAttribute("vanish"))
            m_vanish = parseBool(element.attribute("vanish"));

        SetMinSize(parsePoint(element));
    }
    else if (element.tagName() == "alpha")
    {
        m_effects.m_alpha = getFirstText(element).toInt();
        m_alphaChangeMode = 0;
    }
    else if (element.tagName() == "alphapulse")
    {
        m_alphaChangeMode = 2;
        m_alphaMin = element.attribute("min", "0").toInt();
        m_effects.m_alpha = m_alphaMax = element.attribute("max", "255").toInt();

        if (m_alphaMax > 255)
            m_effects.m_alpha = m_alphaMax = 255;

        m_alphaMin = std::max(m_alphaMin, 0);

        m_alphaChange = element.attribute("change", "5").toInt();
    }
    else if (element.tagName() == "focusorder")
    {
        int order = getFirstText(element).toInt();
        SetFocusOrder(order);
    }
    else if (element.tagName() == "loadondemand")
    {
        SetDeferLoad(parseBool(element));
    }
    else if (element.tagName() == "helptext")
    {
        m_helptext = getFirstText(element);
    }
    else if (element.tagName() == "animation")
    {
        MythUIAnimation::ParseElement(element, this);
    }
    else {
        if (showWarnings) {
            VERBOSE_XML(VB_GENERAL, LOG_ERR, filename, element,
                        QString("Unknown widget type '%1'").arg(element.tagName()));
        }
        return false;
    }

    return true;
}

/**
 *  \brief Perform any post-xml parsing initialisation tasks
 *
 *  This is called after the widget has been created and it's state established
 *  by ParseElement() or CopyFrom(). A derived class should use this to perform
 *  any initialisation tasks which should occur after this point.
 */
void MythUIType::Finalize(void)
{
}

MythFontProperties *MythUIType::GetFont(const QString &text) const
{
    MythFontProperties *ret = m_fonts->GetFont(text);

    if (!ret && m_parent)
        return m_parent->GetFont(text);

    return ret;
}

bool MythUIType::AddFont(const QString &text, MythFontProperties *fontProp)
{
    return m_fonts->AddFont(text, fontProp);
}

void MythUIType::RecalculateArea(bool recurse)
{
    if (m_parent)
        m_area.CalculateArea(m_parent->GetFullArea());
    else
        m_area.CalculateArea(GetMythMainWindow()->GetUIScreenRect());

    if (recurse)
    {
        QList<MythUIType *>::iterator it;

        for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
        {
            (*it)->RecalculateArea(recurse);
        }
    }
}

void MythUIType::SetFocusOrder(int order)
{
    m_focusOrder = order;
}

bool MythUIType::MoveChildToTop(MythUIType *child)
{
    if (!child)
        return false;

    int i = m_childrenList.indexOf(child);

    if (i != -1 || i != m_childrenList.size() - 1)
    {
        m_childrenList.removeAt(i);
        m_childrenList.append(child);
        child->SetRedraw();
        return true;
    }

    return false;
}


bool MythUIType::MoveToTop(void)
{
    if (m_parent)
    {
        return m_parent->MoveChildToTop(this);
    }

    return false;
}

bool MythUIType::IsDeferredLoading(bool recurse) const
{
    if (m_deferload)
        return true;

    if (recurse && m_parent)
        return m_parent->IsDeferredLoading(recurse);

    return false;
}

/**
 *  \brief Cause images in this and child widgets to be loaded. Used only in
 *         conjunction with delayed loading in some large statetypes to
 *         conserve memory.
 */
void MythUIType::LoadNow(void)
{
    QList<MythUIType *>::Iterator it;

    for (it = m_childrenList.begin(); it != m_childrenList.end(); ++it)
        (*it)->LoadNow();
}

/**
 *  \brief Check if the given point falls within this widgets area
 *
 *  Largely used For correctly handling mouse clicks
 */
bool MythUIType::ContainsPoint(const QPoint point) const
{
    return m_area.contains(point);
}

MythPainter *MythUIType::GetPainter(void)
{
    if (m_painter)
        return m_painter;

    if (m_parent)
        return m_parent->GetPainter();

    return GetMythPainter();
}

void MythUIType::SetDependsMap(QMap<QString, QString> dependsMap)
{
    m_dependsMap = std::move(dependsMap);
}

void MythUIType::SetReverseDependence(MythUIType *dependee, bool reverse)
{
    m_reverseDepend.insert(dependee, reverse);
}

void MythUIType::ConnectDependants(bool recurse)
{
    QMapIterator<QString, QString> it(m_dependsMap);
    QStringList dependees;
    QList<int> operators;
    while(it.hasNext())
    {
        it.next();

        // build list of operators and dependeees.
        dependees.clear();
        operators.clear();
        QString name = it.value();
        QStringList tmp1 = name.split("&");
        for (const QString& t1 : std::as_const(tmp1))
        {
            QStringList tmp2 = t1.split("|");

            dependees.append(tmp2[0]);
            for (int j = 1; j < tmp2.size(); j++)
            {
                dependees.append(tmp2[j]);
                operators.append(1); // 1 is OR
            }
            operators.append(2);     // 2 is AND
        }

        MythUIType *dependant = GetChild(it.key());
        if (dependant)
        {
            dependant->m_dependOperator = operators;
            for (QString dependeeName : std::as_const(dependees))
            {
                bool reverse = false;
                if (dependeeName.startsWith('!'))
                {
                    reverse = true;
                    dependeeName.remove(0,1);
                }
                MythUIType *dependee = GetChild(dependeeName);

                if (dependee)
                {
                    QObject::connect(dependee, &MythUIType::DependChanged,
                                     dependant, qOverload<bool>(&MythUIType::UpdateDependState));
                    dependant->SetReverseDependence(dependee, reverse);
                    dependant->m_dependsValue.append(QPair<MythUIType *, bool>(dependee, false));
                    dependant->UpdateDependState(dependee, true);
                }
                else
                {
                    dependant->m_dependsValue.append(QPair<MythUIType *, bool>(dependee, !reverse));
                    dependant->UpdateDependState(dependee, reverse);
                }
            }
        }
    }

    if (recurse)
    {
        QList<MythUIType *>::iterator child;
        for (child = m_childrenList.begin(); child != m_childrenList.end(); ++child)
        {
            if (*child)
                (*child)->ConnectDependants(recurse);
        }
    }
}
