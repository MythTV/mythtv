
// Own header
#include "mythuitype.h"

// QT headers
#include <QEvent>
#include <QKeyEvent>
#include <QDomDocument>

// Mythdb headers
#include "mythverbose.h"

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
#include "mythuiwebbrowser.h"

MythUIType::MythUIType(QObject *parent, const QString &name)
    : QObject(parent)
{
    setObjectName(name);

    m_Visible = true;
    m_Enabled = true;
    m_Initiator = false;
    m_Vanish = false;
    m_Vanished = false;
    m_CanHaveFocus = m_HasFocus = false;
    m_Area = MythRect(0, 0, 0, 0);
    m_MinArea = MythRect(0, 0, 0, 0);
    m_NeedsRedraw = false;
    m_Alpha = 255;
    m_AlphaChangeMode = m_AlphaChange = m_AlphaMin = 0;
    m_AlphaMax = 255;
    m_Moving = false;
    m_XYDestination = QPoint(0, 0);
    m_XYSpeed = QPoint(0, 0);
    m_deferload = false;

    m_Parent = NULL;
    if (parent)
    {
        m_Parent = dynamic_cast<MythUIType *>(parent);
        if (m_Parent)
            m_Parent->AddChild(this);
    }

    m_DirtyRegion = QRegion(QRect(0, 0, 0, 0));

    m_Fonts = new FontMap();
    m_focusOrder = 0;
    m_Painter = NULL;

    m_BorderColor = QColor(rand() % 255, rand()  % 255, rand()  % 255);
}

MythUIType::~MythUIType()
{
    delete m_Fonts;
}

/**
 *  \brief Reset the widget to it's original state, should not reset changes
 *         made by the theme
 */
void MythUIType::Reset()
{
    // Reset all children
    QMutableListIterator<MythUIType *> it(m_ChildrenList);
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

    m_ChildrenList.push_back(child);
}

static QObject *qChildHelper(const char *objName, const char *inheritsClass,
                             bool recursiveSearch, const QObjectList &children)
{
    if (children.isEmpty())
        return 0;

    bool onlyWidgets = (inheritsClass
                        && qstrcmp(inheritsClass, "QWidget") == 0);
    const QLatin1String oName(objName);
    for (int i = 0; i < children.size(); ++i)
    {
        QObject *obj = children.at(i);

        if (onlyWidgets)
        {
            if (obj->isWidgetType() && (!objName || obj->objectName() == oName))
                return obj;
        }
        else if ((!inheritsClass || obj->inherits(inheritsClass))
                   && (!objName || obj->objectName() == oName))
            return obj;
        if (recursiveSearch && (dynamic_cast<MythUIGroup *>(obj) != NULL)
                            && (obj = qChildHelper(objName, inheritsClass,
                                                   recursiveSearch,
                                                   obj->children())))
            return obj;
    }
    return 0;
}

/**
 *  \brief Get a named child of this UIType
 *
 *  \param Name of child
 *  \return Pointer to child if found, or NULL
 */
MythUIType *MythUIType::GetChild(const QString &name) const
{
    QObject *ret = qChildHelper(name.toAscii().constData(), NULL, true, children());
    if (ret)
        return dynamic_cast<MythUIType *>(ret);

    return NULL;
}

/**
 *  \brief Delete a named child of this UIType
 *
 *  \param Name of child
 */
void MythUIType::DeleteChild(const QString &name)
{
    QMutableListIterator<MythUIType *> it(m_ChildrenList);
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
 *  NULL if successful.
 */
void MythUIType::DeleteChild(MythUIType *child)
{
    QMutableListIterator<MythUIType *> it(m_ChildrenList);
    while (it.hasNext())
    {
        it.next();
        MythUIType *type = it.value();
        if (type == child)
        {
            delete type;
            it.remove();
            child = NULL;
            return;
        }
    }
}

/**
 *  \brief Return a list of all child widgets
 */
QList<MythUIType *> *MythUIType::GetAllChildren(void)
{
    return &m_ChildrenList;
}

/**
 *  \brief Delete all child widgets
 */
void MythUIType::DeleteAllChildren(void)
{
    QList<MythUIType*>::iterator it;
    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
        delete *it;

    m_ChildrenList.clear();
}

/** \brief Return the first MythUIType which accepts focus found at the given
 *         coordinates
 *
 *  \param p QPoint coordinates
 *  \param recursive Whether to perform a recursive search
 *
 *  \return The widget at these coordinates
 */
MythUIType *MythUIType::GetChildAt(const QPoint &p, bool recursive,
                                   bool focusable) const
{
    if (GetArea().contains(p))
    {
        if (!IsVisible() || !IsEnabled())
            return NULL;

        if (m_ChildrenList.isEmpty())
            return NULL;

        /* check all children */
        QList<MythUIType *>::const_iterator it;
        for (it = m_ChildrenList.end()-1; it != m_ChildrenList.begin()-1; --it)
        {
            if (!(*it))
                continue;

            MythUIType *child = NULL;


            if ((*it)->GetArea().contains(p - GetArea().topLeft()))
                child = *it;

            if (!child && recursive)
                child = (*it)->GetChildAt(p - GetArea().topLeft(), recursive,
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
    return NULL;
}

bool MythUIType::NeedsRedraw(void) const
{
    return m_NeedsRedraw;
}

void MythUIType::ResetNeedsRedraw(void)
{
    m_NeedsRedraw = false;

    QList<MythUIType *>::Iterator it;
    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
        (*it)->ResetNeedsRedraw();
}

void MythUIType::SetRedraw(void)
{
    if (m_Area.width() == 0 || m_Area.height() == 0)
        return;

    m_NeedsRedraw = true;

    if (m_DirtyRegion.isEmpty())
        m_DirtyRegion = QRegion(m_Area.toQRect());
    else
        m_DirtyRegion = m_DirtyRegion.unite(QRegion(m_Area.toQRect()));

    if (m_Parent)
        m_Parent->SetChildNeedsRedraw(this);
}

void MythUIType::SetChildNeedsRedraw(MythUIType *child)
{
    QRegion childRegion = child->GetDirtyArea();
    if (childRegion.isEmpty())
        return;

    childRegion.translate(m_Area.x(), m_Area.y());

    childRegion = childRegion.intersect(m_Area.toQRect());

    m_NeedsRedraw = true;

    if (m_DirtyRegion.isEmpty())
        m_DirtyRegion = childRegion;
    else
        m_DirtyRegion = m_DirtyRegion.unite(childRegion);

    if (m_Parent)
        m_Parent->SetChildNeedsRedraw(this);
}

/**
 *  \brief Return if this widget can accept input focus
 */
bool MythUIType::CanTakeFocus(void) const
{
    return m_CanHaveFocus;
}

/**
 *  \brief Set whether this widget can take focus
 */
void MythUIType::SetCanTakeFocus(bool set)
{
    m_CanHaveFocus = set;
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

    if (!m_Moving)
        return;

    QPoint curXY = m_Area.topLeft().toQPoint();
    m_DirtyRegion = m_Area.toQRect();

    int xdir = m_XYDestination.x() - curXY.x();
    int ydir = m_XYDestination.y() - curXY.y();

    curXY.setX(curXY.x() + m_XYSpeed.x());
    curXY.setY(curXY.y() + m_XYSpeed.y());

    if ((xdir > 0 && curXY.x() >= m_XYDestination.x()) ||
        (xdir < 0 && curXY.x() <= m_XYDestination.x()) ||
        (xdir == 0))
    {
        m_XYSpeed.setX(0);
    }

    if ((ydir > 0 && curXY.y() >= m_XYDestination.y()) ||
        (ydir <= 0 && curXY.y() <= m_XYDestination.y()) ||
        (ydir == 0))
    {
        m_XYSpeed.setY(0);
    }

    SetRedraw();

    if (m_XYSpeed.x() == 0 && m_XYSpeed.y() == 0)
    {
        m_Moving = false;
        emit FinishedMoving();
    }

    m_Area.moveTopLeft(curXY);
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

    if (m_AlphaChangeMode == 0)
        return;

    m_Alpha += m_AlphaChange;

    if (m_Alpha > m_AlphaMax)
        m_Alpha = m_AlphaMax;
    if (m_Alpha < m_AlphaMin)
        m_Alpha = m_AlphaMin;

    // Reached limits so change direction
    if (m_Alpha == m_AlphaMax || m_Alpha == m_AlphaMin)
    {
        if (m_AlphaChangeMode == 2)
        {
            m_AlphaChange *= -1;
        }
        else
        {
            m_AlphaChangeMode = 0;
            m_AlphaChange = 0;
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
    if (!m_Visible || m_Vanished)
        return;

    HandleMovementPulse();
    HandleAlphaPulse();

    QList<MythUIType *>::Iterator it;
    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
        (*it)->Pulse();
}

int MythUIType::CalcAlpha(int alphamod)
{
    return (int)(m_Alpha * (alphamod / 255.0));
}

void MythUIType::DrawSelf(MythPainter *, int, int, int, QRect)
{
}

void MythUIType::Draw(MythPainter *p, int xoffset, int yoffset, int alphaMod,
                      QRect clipRect)
{
    m_DirtyRegion = QRegion(QRect(0,0,0,0));

    if (!m_Visible || m_Vanished)
        return;

    QRect realArea = m_Area.toQRect();
    realArea.translate(xoffset, yoffset);

    if (!realArea.intersects(clipRect))
        return;

    DrawSelf(p, xoffset, yoffset, alphaMod, clipRect);

    QList<MythUIType *>::Iterator it;
    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
    {
        (*it)->Draw(p, xoffset + m_Area.x(), yoffset + m_Area.y(),
                    CalcAlpha(alphaMod), clipRect);
    }

    if (p->ShowBorders())
    {
        static const QBrush nullbrush(Qt::NoBrush);
        p->DrawRect(realArea, nullbrush, QPen(m_BorderColor), 255);

        if (p->ShowTypeNames())
        {
            MythFontProperties font;
            font.SetFace(QFont("Droid Sans"));
            font.SetColor(m_BorderColor);
            font.SetPointSize(8);
            p->DrawText(realArea, objectName(), 0, font, 255, realArea);
        }
    }
}

void MythUIType::SetPosition(int x, int y)
{
    SetPosition(MythPoint(x, y));
}

void MythUIType::SetPosition(const MythPoint &pos)
{
    if (m_Area.topLeft() == pos)
        return;

    m_DirtyRegion = QRegion(m_Area.toQRect());

    m_Area.moveTopLeft(pos);

    RecalculateArea(false);

    SetRedraw();
}

MythPoint MythUIType::GetPosition(void) const
{
    return m_Area.topLeft();
}

void MythUIType::SetSize(const QSize &size)
{
    if (size == m_Area.size())
        return;

    m_DirtyRegion = QRegion(m_Area.toQRect());

    m_Area.setSize(size);
    RecalculateArea();

    if (m_Parent)
        m_Parent->ExpandArea(m_Area.toQRect());

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

    if (m_Parent)
        point.CalculatePoint(m_Parent->GetArea());

    m_MinSize = point;
}

QSize MythUIType::GetMinSize(void) const
{
    if (!m_MinSize.isValid())
        return m_Area.size();

    return QSize(m_MinSize.x(), m_MinSize.y());
}

void MythUIType::SetArea(const MythRect &rect)
{
    if (rect == m_Area)
        return;

    m_DirtyRegion = QRegion(m_Area.toQRect());

    m_Area = rect;

    if (m_Area.width() > m_NormalSize.width())
        m_NormalSize.setWidth(m_Area.width());
    if (m_Area.height() > m_NormalSize.height())
        m_NormalSize.setHeight(m_Area.height());

    RecalculateArea();

    if (m_Parent)
        m_Parent->ExpandArea(m_Area.toQRect());

    SetRedraw();
}

/**
 * Adjust the size of a sibling.
 */
void MythUIType::AdjustMinArea(int delta_x, int delta_y)
{
    // If a minsize is not set, don't use MinArea
    if (!m_MinSize.isValid())
        return;

    if (m_Area.width() < m_NormalSize.width())
        m_Area.setWidth(m_NormalSize.width());
    if (m_Area.height() < m_NormalSize.height())
        m_Area.setHeight(m_NormalSize.height());

    QSize size(m_Area.size());

    size.setWidth( size.width()  + delta_x);
    size.setHeight(size.height() + delta_y);
    m_MinArea.setSize(size);
    m_MinArea.moveLeft(m_Area.x());
    m_MinArea.moveTop(m_Area.y());

    QSize bound(m_MinArea.width(), m_MinArea.height());
    bound = bound.expandedTo(GetMinSize());

    m_MinArea.setWidth(bound.width());
    m_MinArea.setHeight(bound.height());
    m_Vanished = false;
}

void MythUIType::VanishSibling(void)
{
    if (!m_MinSize.isValid() || !m_Vanish)
        return;

    m_MinArea.moveLeft(0);
    m_MinArea.moveTop(0);
    m_MinArea.setWidth(0);
    m_MinArea.setHeight(0);
    m_Vanished = true;
}

/**
 * Adjust the size of sibling objects within the button.
 */
void MythUIType::SetMinAreaParent(MythRect actual_area, MythRect allowed_area,
                                  const MythUIType *calling_child)
{
    int delta_x, delta_y;
    MythRect area;

    // If a minsize is not set, don't use MinArea
    if (!m_MinSize.isValid())
        return;

    m_MinArea.setWidth(0);

    if (m_Area.width() < m_NormalSize.width())
        m_Area.setWidth(m_NormalSize.width());
    if (m_Area.height() < m_NormalSize.height())
        m_Area.setHeight(m_NormalSize.height());

    if (calling_child->m_Vanished)
    {
        actual_area.moveLeft(0);
        actual_area.moveTop(0);
        allowed_area.moveLeft(0);
        allowed_area.moveTop(0);
    }

    actual_area.translate(m_Area.topLeft());
    allowed_area.translate(m_Area.topLeft());

    QList<MythUIType*>::iterator it;

    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
    {
        if (*it == calling_child || !(*it)->m_Initiator)
            continue;

        // Find union of area(s)
        area = (*it)->GetArea();
        area.translate(m_Area.topLeft());
        actual_area = actual_area.united(area);

        area = (*it)->m_Area;
        area.translate(m_Area.topLeft());
        allowed_area = allowed_area.united(area);
    }

    // Make sure it is not larger than the area allowed
    actual_area = actual_area.intersected(m_Area);
    allowed_area = allowed_area.intersected(m_Area);

    if (m_Vanish && actual_area.size().isNull())
    {
        m_Vanished = true;
    }
    else
    {
        if (calling_child->m_Vanished)
        {
            delta_x = (actual_area.x() + actual_area.width()) -
                      (m_Area.x() + m_Area.width());
            delta_y = (actual_area.y() + actual_area.height()) -
                      (m_Area.y() + m_Area.height());
        }
        else
        {
            delta_x = (actual_area.x() + actual_area.width()) -
                      (allowed_area.x() + allowed_area.width());
            delta_y = (actual_area.y() + actual_area.height()) -
                      (allowed_area.y() + allowed_area.height());
        }
        m_Vanished = false;
    }

    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
    {
        if (*it == calling_child)
            continue;

        if (!(*it)->m_Initiator)
        {
            if (m_Vanished)
                (*it)->VanishSibling();
            else
                (*it)->AdjustMinArea(delta_x, delta_y);
        }

        area = (*it)->GetArea();
        area.translate(m_Area.topLeft());
        actual_area = actual_area.united(area);
    }

    QSize bound(actual_area.width(), actual_area.height());
    if (m_Vanished)
    {
        m_MinArea.moveLeft(0);
        m_MinArea.moveTop(0);
    }
    else
        bound = bound.expandedTo(GetMinSize());

    m_MinArea.setWidth(bound.width());
    m_MinArea.setHeight(bound.height());

    if (m_Parent)
        m_Parent->SetMinAreaParent(actual_area, m_Area, this);
}

/**
 * Set the minimum area based on the given size
 */
void MythUIType::SetMinArea(const QSize &size)
{
    // If a minsize is not set, don't use MinArea
    if (!m_Initiator || !m_MinSize.isValid())
        return;

    m_MinArea.setWidth(0);
    if (m_Area.width() < m_NormalSize.width())
        m_Area.setWidth(m_NormalSize.width());
    if (m_Area.height() < m_NormalSize.height())
        m_Area.setHeight(m_NormalSize.height());

    /**
     * The MinArea will have the same origin as the normal Area,
     * but can have a smaller size.
     */
    QSize bounded(size);

    if (!bounded.isNull() || !m_Vanish)
    {
        if (bounded.isNull())
            bounded = GetMinSize();
        else
            bounded = bounded.expandedTo(GetMinSize());

        bounded = bounded.boundedTo(m_Area.size());
    }

    if (m_Vanish == m_Vanished && bounded == m_MinArea.size())
        return;

    m_MinArea.setWidth(bounded.width());
    m_MinArea.setHeight(bounded.height());
    m_Vanished = (m_Vanish && m_MinArea.size().isNull());

    if (m_Vanished)
    {
        m_MinArea.moveLeft(0);
        m_MinArea.moveTop(0);
    }
    else
    {
        m_MinArea.moveLeft(m_Area.x());
        m_MinArea.moveTop(m_Area.y());
    }

    if (m_Parent)
        m_Parent->SetMinAreaParent(m_MinArea, m_Area, this);
}

void MythUIType::ExpandArea(const MythRect &rect)
{
    QSize childSize = rect.size();
    QSize size = m_Area.size();

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
    if (m_Vanished || m_MinArea.isValid())
        return m_MinArea;
    return m_Area;
}

QRegion MythUIType::GetDirtyArea(void) const
{
    return m_DirtyRegion;
}

bool MythUIType::IsVisible(bool recurse) const
{
    if (recurse)
    {
        if (m_Parent && !m_Parent->IsVisible(recurse))
            return false;
    }

    return m_Visible;
}

void MythUIType::MoveTo(QPoint destXY, QPoint speedXY)
{
    if (!GetPainter()->SupportsAnimation())
        return;

    if (destXY.x() == m_Area.x() && destXY.y() == m_Area.y())
        return;

    m_Moving = true;

    m_XYDestination = destXY;
    m_XYSpeed = speedXY;
}

void MythUIType::AdjustAlpha(int mode, int alphachange, int minalpha,
                             int maxalpha)
{
    if (!GetPainter()->SupportsAlpha())
        return;

    m_AlphaChangeMode = mode;
    m_AlphaChange = alphachange;
    m_AlphaMin = minalpha;
    m_AlphaMax = maxalpha;

    if (m_Alpha > m_AlphaMax)
        m_Alpha = m_AlphaMax;
    if (m_Alpha < m_AlphaMin)
        m_Alpha = m_AlphaMin;
}

void MythUIType::SetAlpha(int newalpha)
{
    if (m_Alpha == newalpha)
        return;
    m_Alpha = newalpha;
    SetRedraw();
}

int MythUIType::GetAlpha(void) const
{
    return m_Alpha;
}

/** \brief Key event handler
 *
 *  \param event Keypress event
 */
bool MythUIType::keyPressEvent(QKeyEvent *)
{
    return false;
}


void MythUIType::customEvent(QEvent *)
{
    return;
}

/** \brief Mouse click/movement handler, receives mouse gesture events from the
 *         QCoreApplication event loop. Should not be used directly.
 *
 *  \param event Mouse event
 */
bool MythUIType::gestureEvent(MythGestureEvent *)
{
    return false;
}

/** \brief Media/Device status event handler, received from MythMediaMonitor
 *
 *  \param event Media event
 */
void MythUIType::mediaEvent (MythMediaEvent*)
{
    return;
}

void MythUIType::LoseFocus(void)
{
    if (!m_CanHaveFocus || !m_HasFocus)
        return;

    emit LosingFocus();
    m_HasFocus = false;
    Refresh();
}

bool MythUIType::TakeFocus(void)
{
    if (!m_CanHaveFocus || m_HasFocus)
        return false;

    m_HasFocus = true;
    Refresh();
    emit TakingFocus();
    return true;
}

void MythUIType::Activate(void)
{
}

void MythUIType::Refresh(void)
{
    SetRedraw();
}

void MythUIType::SetVisible(bool visible)
{
    if (visible == m_Visible)
        return;

    m_Visible = visible;
    SetRedraw();

    if (m_Visible)
        emit Showing();
    else
        emit Hiding();
}

void MythUIType::SetEnabled(bool enable)
{
    if (m_Enabled != enable)
        m_Enabled = enable;

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

void MythUIType::AddFocusableChildrenToList(QMap<int, MythUIType *> &focusList)
{
    if (m_CanHaveFocus)
        focusList.insertMulti(m_focusOrder, this);

    QList<MythUIType *>::Iterator it;
    for (it = m_ChildrenList.end()-1; it != m_ChildrenList.begin()-1; --it)
        (*it)->AddFocusableChildrenToList(focusList);
}

int MythUIType::NormX(const int x)
{
    return GetMythMainWindow()->NormX(x);
}

int MythUIType::NormY(const int y)
{
    return GetMythMainWindow()->NormY(y);
}

/**
 *  \brief Copy this widgets state from another.
 */
void MythUIType::CopyFrom(MythUIType *base)
{
    m_Visible = base->m_Visible;
    m_Enabled = base->m_Enabled;
    m_CanHaveFocus = base->m_CanHaveFocus;
    m_focusOrder = base->m_focusOrder;

    SetArea(base->m_Area);
    m_MinArea = base->m_MinArea;
    m_MinSize = base->m_MinSize;
    m_Initiator = base->m_Initiator;
    m_Vanish = base->m_Vanish;
    m_Vanished = base->m_Vanished;
    m_NormalSize = base->m_NormalSize;
    m_Alpha = base->m_Alpha;
    m_AlphaChangeMode = base->m_AlphaChangeMode;
    m_AlphaChange = base->m_AlphaChange;
    m_AlphaMin = base->m_AlphaMin;
    m_AlphaMax = base->m_AlphaMax;

    m_Moving = base->m_Moving;
    m_XYDestination = base->m_XYDestination;
    m_XYSpeed = base->m_XYSpeed;
    m_deferload = base->m_deferload;

    QList<MythUIType *>::Iterator it;
    for (it = base->m_ChildrenList.begin(); it != base->m_ChildrenList.end();
         ++it)
    {
        MythUIType *child = GetChild((*it)->objectName());
        if (child)
            child->CopyFrom(*it);
        else
            (*it)->CreateCopy(this);
    }
}

/**
 *  \brief Copy the state of this widget to the one given, it must be of the
 *         same type.
 */
void MythUIType::CreateCopy(MythUIType *)
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
            m_Initiator = parseBool(element.attribute("initiator"));
        if (element.hasAttribute("vanish"))
            m_Vanish = parseBool(element.attribute("vanish"));
        SetMinSize(parsePoint(element));
    }
    else if (element.tagName() == "alpha")
    {
        m_Alpha = getFirstText(element).toInt();
        m_AlphaChangeMode = 0;
    }
    else if (element.tagName() == "alphapulse")
    {
        m_AlphaChangeMode = 2;
        m_AlphaMin = element.attribute("min", "0").toInt();
        m_Alpha = m_AlphaMax = element.attribute("max", "255").toInt();
        if (m_AlphaMax > 255)
            m_Alpha = m_AlphaMax = 255;
        if (m_AlphaMin < 0)
            m_AlphaMin = 0;
        m_AlphaChange = element.attribute("change", "5").toInt();
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
    else
        return false;

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
    MythFontProperties *ret = m_Fonts->GetFont(text);
    if (!ret && m_Parent)
        return m_Parent->GetFont(text);

    return ret;
}

bool MythUIType::AddFont(const QString &text, MythFontProperties *fontProp)
{
    return m_Fonts->AddFont(text, fontProp);
}

void MythUIType::RecalculateArea(bool recurse)
{
    if (m_Parent)
        m_Area.CalculateArea(m_Parent->GetArea());
    else
        m_Area.CalculateArea(GetMythMainWindow()->GetUIScreenRect());

    if (m_Area.width() > m_NormalSize.width())
        m_NormalSize.setWidth(m_Area.width());
    if (m_Area.height() > m_NormalSize.height())
        m_NormalSize.setHeight(m_Area.height());

    if (recurse)
    {
        QList<MythUIType *>::iterator it;
        for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
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

    int i = m_ChildrenList.indexOf(child);
    if (i != -1 || i != m_ChildrenList.size()-1)
    {
        m_ChildrenList.removeAt(i);
        m_ChildrenList.append(child);
        child->SetRedraw();
        return true;
    }

    return false;
}


bool MythUIType::MoveToTop(void)
{
    if (m_Parent)
    {
        return m_Parent->MoveChildToTop(this);
    }

    return false;
}

bool MythUIType::IsDeferredLoading(bool recurse) const
{
     if (m_deferload)
         return true;

     if (recurse && m_Parent)
         return m_Parent->IsDeferredLoading(recurse);

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
    for (it = m_ChildrenList.begin(); it != m_ChildrenList.end(); ++it)
        (*it)->LoadNow();
}

/**
 *  \brief Check if the given point falls within this widgets area
 *
 *  Largely used For correctly handling mouse clicks
 */
bool MythUIType::ContainsPoint(const QPoint &point) const
{
    if (m_Area.contains(point))
        return true;

    return false;
}

MythPainter* MythUIType::GetPainter(void)
{
    if (m_Painter)
        return m_Painter;
    if (m_Parent)
        return m_Parent->GetPainter();
    return GetMythPainter();
}
