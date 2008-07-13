// C++ headers
#include <cassert>
#include <iostream>

using namespace std;

// QT headers
#include <QApplication>
#include <QPainter>
#include <QPixmap>

// libmyth headers
#include "mythverbose.h"

// mythui headers
#include "mythuibuttonlist.h"
#include "mythuigroup.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"
#include "mythuistatetype.h"

MythUIButtonList::MythUIButtonList(MythUIType *parent, const QString &name)
              : MythUIType(parent, name)
{
    m_showArrow = true;
    m_showScrollArrows = false;

    Const();
}

MythUIButtonList::MythUIButtonList(MythUIType *parent, const QString &name,
                               const QRect& area, bool showArrow,
                               bool showScrollArrows)
              : MythUIType(parent, name)
{
    m_Area             = area;

    m_showArrow        = showArrow;
    m_showScrollArrows = showScrollArrows;

    Const();
}

void MythUIButtonList::Const(void)
{
    m_contentsRect = QRect(0,0,0,0);

    m_layout      = LayoutVertical;
    m_scrollStyle = ScrollFree;

    m_active           = false;
    m_drawFromBottom   = false;

    m_topItem = 0;
    m_selItem = 0;
    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount = 0;

    m_initialized      = false;
    m_clearing         = false;
    m_itemHorizSpacing = 0;
    m_itemVertSpacing  = 0;
    m_itemHeight       = 0;
    m_itemWidth       = 0;
    m_itemsVisible     = 0;
    m_columns          = 0;
    m_rows             = 0;

    m_upArrow = m_downArrow = NULL;

    SetCanTakeFocus(true);

    connect(this, SIGNAL(TakingFocus()), this, SLOT(Select()));
    connect(this, SIGNAL(LosingFocus()), this, SLOT(Deselect()));
}

MythUIButtonList::~MythUIButtonList()
{
    m_clearing = true;
    while (!m_itemList.isEmpty())
        delete m_itemList.takeFirst();
}

void MythUIButtonList::Select()
{
    MythUIButtonListItem *item = GetItemCurrent();
    if (item)
        emit itemSelected(item);
    SetActive(true);
}

void MythUIButtonList::Deselect()
{
    SetActive(false);
}

void MythUIButtonList::SetSpacing(int spacing)
{
    m_itemHorizSpacing = NormX(spacing);
    m_itemVertSpacing = NormY(spacing);
}

void MythUIButtonList::SetDrawFromBottom(bool draw)
{
    m_drawFromBottom = draw;
}

void MythUIButtonList::SetActive(bool active)
{
    m_active = active;
    if (m_initialized)
        SetPositionArrowStates();
}

void MythUIButtonList::Reset()
{
    m_clearing = true;

    while (!m_itemList.isEmpty())
        delete m_itemList.takeFirst();

    m_clearing = false;

    m_topItem     = 0;
    m_selItem     = 0;
    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount   = 0;

    SetPositionArrowStates();
}

void MythUIButtonList::Update()
{
    if (m_initialized)
        SetPositionArrowStates();
}

void MythUIButtonList::SetPositionArrowStates(void)
{
    if (!m_initialized)
        Init();

    if (m_ButtonList.size() > 0)
    {
        int button = 0;

        if ((m_scrollStyle == ScrollCenter) && m_selPosition <= (int)(m_itemsVisible/2))
        {
            button = (m_itemsVisible / 2) - m_selPosition;
        }
        else if (m_drawFromBottom && m_itemCount < (int)m_itemsVisible)
            button = m_itemsVisible - m_itemCount;

        for (int i = 0; i < button; i++)
             m_ButtonList[i]->SetVisible(false);

        QList<MythUIButtonListItem*>::iterator it = m_itemList.begin() + m_topPosition;
        while (it != m_itemList.end() && button < (int)m_itemsVisible)
        {
            MythUIStateType *realButton = m_ButtonList[button];
            MythUIButtonListItem *buttonItem = *it;

            buttonItem->SetToRealButton(realButton, true);
            realButton->SetVisible(true);

            ++it;
            button++;
        }

        for (; button < (int)m_itemsVisible; button++)
            m_ButtonList[button]->SetVisible(false);

    }

    if (!m_showScrollArrows || !m_downArrow || !m_upArrow)
        return;

    if (m_itemCount == 0)
    {
        m_downArrow->DisplayState(MythUIStateType::Off);
        m_upArrow->DisplayState(MythUIStateType::Off);
    }
    else
    {
        if (m_topItem != m_itemList.first())
            m_upArrow->DisplayState(MythUIStateType::Full);
        else
            m_upArrow->DisplayState(MythUIStateType::Off);

        if (m_topPosition + (int)m_itemsVisible < m_itemCount)
            m_downArrow->DisplayState(MythUIStateType::Full);
        else
            m_downArrow->DisplayState(MythUIStateType::Off);
    }
}

void MythUIButtonList::InsertItem(MythUIButtonListItem *item)
{
    bool wasEmpty = m_itemList.isEmpty();
    m_itemList.append(item);

    m_itemCount++;

    if (wasEmpty)
    {
        m_topItem = item;
        m_selItem = item;
        m_selPosition = m_topPosition = 0;

        emit itemSelected(item);
    }

    SetPositionArrowStates();
}

void MythUIButtonList::RemoveItem(MythUIButtonListItem *item)
{
    if (m_clearing)
        return;

    int curIndex = m_itemList.indexOf(item);
    if (curIndex == -1)
        return;

    if (item == m_topItem)
    {
        if (m_topItem != m_itemList.last())
        {
            ++m_topPosition;
            m_topItem = *(m_itemList.begin() + m_topPosition);
        }
        else if (m_topItem != m_itemList.first())
        {
            --m_topPosition;
            m_topItem = *(m_itemList.begin() + m_topPosition);
        }
        else
        {
            m_topItem = NULL;
            m_topPosition = 0;
        }
    }

    if (item == m_selItem)
    {
        if (m_selItem != m_itemList.last())
        {
            ++m_selPosition;
            m_selItem = *(m_itemList.begin() + m_selPosition);
        }
        else if (m_selItem != m_itemList.first())
        {
            --m_selPosition;
            m_selItem = *(m_itemList.begin() + m_selPosition);
        }
        else
        {
            m_selItem = NULL;
            m_selPosition = 0;
        }
    }

    m_itemList.removeAt(curIndex);
    m_itemCount--;

    SetPositionArrowStates();

    if (m_selItem)
        emit itemSelected(m_selItem);
}

void MythUIButtonList::SetItemCurrent(MythUIButtonListItem* item)
{
    m_selPosition = m_itemList.indexOf(item);

    if (m_selPosition == -1)
        m_selPosition = 0;

    m_selItem = m_itemList.at(m_selPosition);

    switch (m_scrollStyle)
    {
        case ScrollCenter :
            while (m_topPosition + (int)((float)m_itemsVisible/2) < m_selPosition + 1)
                ++m_topPosition;

            break;
        case ScrollFree :
            while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1)
                ++m_topPosition;

            break;
    }

    m_topItem = m_itemList.at(m_topPosition);

    SetPositionArrowStates();

    emit itemSelected(m_selItem);
}

void MythUIButtonList::SetItemCurrent(int current)
{
    if (current >= m_itemList.size())
        return;

    MythUIButtonListItem* item = m_itemList.at(current);
    if (!item)
        item = m_itemList.first();

    SetItemCurrent(item);
}

MythUIButtonListItem* MythUIButtonList::GetItemCurrent()
{
    return m_selItem;
}

MythUIButtonListItem* MythUIButtonList::GetItemFirst()
{
    return m_itemList.first();
}

MythUIButtonListItem* MythUIButtonList::GetItemNext(MythUIButtonListItem *item)
{
    QListIterator<MythUIButtonListItem*> it(m_itemList);
    if (!it.findNext(item))
        return 0;

    return it.previous();
}

int MythUIButtonList::GetCount()
{
    return m_itemCount;
}

bool MythUIButtonList::IsEmpty()
{
    if (m_itemCount > 0)
        return false;
    else
        return true;
}

MythUIButtonListItem* MythUIButtonList::GetItemAt(int pos)
{
    return m_itemList.at(pos);
}

int MythUIButtonList::GetItemPos(MythUIButtonListItem* item)
{
    if (!item)
        return -1;
    return m_itemList.indexOf(item);
}

void MythUIButtonList::MoveUp(MovementUnit unit)
{
    int pos = m_selPosition;
    if (pos == -1)
        return;

    switch (unit)
    {
        case MoveItem:
            if (m_selPosition > 0)
            {
                --m_selPosition;
            }
            break;
        case MoveColumn:
            if ((pos+1) - (((pos+1)/m_columns)*m_columns) != 1)
            {
                --m_selPosition;
            }
            break;
        case MoveRow:
            if ((pos - m_columns) >= 0)
            {
                for (int i = 0; i < m_columns; i++)
                {
                    --m_selPosition;
                }
            }
            break;
        case MovePage:
            m_selPosition = max(0, m_selPosition - (int)m_itemsVisible);
            break;
        case MoveMax:
            m_selPosition = 0;
            break;
    }

    if (m_selPosition >= m_itemList.size())
        return;

    m_selItem = m_itemList.at(m_selPosition);

    switch (m_scrollStyle)
    {
        case ScrollCenter :
            while (m_topPosition > 0 && m_topPosition +
                   (int)((float)m_itemsVisible/2) > m_selPosition)
            {
                --m_topPosition;
                m_topItem = m_itemList.at(m_topPosition);
            }

            break;
        case ScrollFree :
            if (m_selPosition < m_topPosition)
            {
                if (m_layout == LayoutHorizontal)
                    m_topPosition = m_selPosition;
                else
                    m_topPosition = m_selPosition / m_columns * m_columns;
            }
            break;
    }

    m_topItem = m_itemList.at(m_topPosition);

    SetPositionArrowStates();

    emit itemSelected(m_selItem);
}

void MythUIButtonList::MoveDown(MovementUnit unit)
{
    int pos = m_selPosition;
    if (pos == -1)
        return;

    switch (unit)
    {
        case MoveItem:
            if (m_selPosition < m_itemList.size() - 1)
            {
                ++m_selPosition;
            }
            break;
        case MoveColumn:
            if ((pos+1) - (((pos+1)/m_columns)*m_columns) > 0)
            {
                ++m_selPosition;
            }
            break;
        case MoveRow:
            if ((pos + m_columns) < m_itemCount)
            {
                for (int i = 0; i < m_columns; i++)
                {
                    ++m_selPosition;
                }
            }
            break;
        case MovePage:
            m_selPosition = min(m_itemCount - 1, m_selPosition + (int)m_itemsVisible);
            break;
        case MoveMax:
            m_selPosition = m_itemCount - 1;
            break;
    }

    if (m_selPosition >= m_itemList.size())
    {
        //VERBOSE(VB_IMPORTANT, "m_selPosition has left the building.");
        return;
    }

    m_selItem = m_itemList.at(m_selPosition);

    switch (m_scrollStyle)
    {
        case ScrollCenter :
            while (m_topPosition + (int)((float)m_itemsVisible/2) < m_selPosition)
                ++m_topPosition;
            break;
        case ScrollFree :
            if (m_topPosition + (int)m_itemsVisible <= m_selPosition)
            {
                if (m_layout == LayoutHorizontal)
                    m_topPosition = m_selPosition - m_itemsVisible + 1;
                else
                    m_topPosition = (m_selPosition - m_itemsVisible + m_columns)
                                        / m_columns * m_columns;

                m_topPosition = max(0, m_topPosition);
            }
            break;
    }

    m_topItem = m_itemList.at(m_topPosition);

    SetPositionArrowStates();

    emit itemSelected(m_selItem);
}

bool MythUIButtonList::MoveToNamedPosition(const QString &position_name)
{
    if (m_selPosition < 0)
        return false;

    if (m_itemList.isEmpty())
        return false;

    bool found_it = false;
    int selectedPosition = 0;
    QList<MythUIButtonListItem*>::iterator it = m_itemList.begin();
    while(it != m_itemList.end())
    {
        if ((*it)->text() == position_name)
        {
            found_it = true;
            break;
        }
        ++it;
        ++selectedPosition;
    }

    if (!found_it)
        return false;

    m_selPosition = selectedPosition;
    m_selItem =  m_itemList.at(m_selPosition);

    m_topPosition = 0;

    switch (m_scrollStyle)
    {
        case ScrollCenter :
            while (m_topPosition + (int)((float)m_itemsVisible/2) < m_selPosition)
                ++m_topPosition;
            break;
        case ScrollFree :
            if (m_topPosition + (int)m_itemsVisible <= m_selPosition)
            {
                if (m_layout == LayoutHorizontal)
                    m_topPosition = m_selPosition - m_itemsVisible + 1;
                else
                    m_topPosition = (m_selPosition - m_itemsVisible + m_columns)
                                        / m_columns * m_columns;

                m_topPosition = max(0, m_topPosition);
            }
            break;
    }

    m_topItem = m_itemList.at(m_topPosition);

    SetPositionArrowStates();

    emit itemSelected(m_selItem);

    return true;
}

bool MythUIButtonList::MoveItemUpDown(MythUIButtonListItem *item, bool flag)
{
    if (item != m_selItem)
    {
        return false;
    }

    if (item == m_itemList.first() && flag)
        return false;
    if (item == m_itemList.last() && !flag)
        return false;

    int oldpos = m_selPosition;
    int insertat = 0;
    bool dolast = false;

    if (flag)
    {
        insertat = m_selPosition - 1;
        if (item == m_itemList.last())
            dolast = true;
        else
            ++m_selPosition;

        if (item == m_topItem)
            ++m_topPosition;
    }
    else
        insertat = m_selPosition + 1;

    m_itemList.removeAt(oldpos);

    m_itemList.insert(insertat, item);

    if (flag)
    {
        MoveUp();
        if (!dolast)
            MoveUp();
    }
    else
        MoveDown();

    return true;
}

void MythUIButtonList::SetAllChecked(MythUIButtonListItem::CheckState state)
{
    QMutableListIterator<MythUIButtonListItem*> it(m_itemList);
    while (it.hasNext())
        it.next()->setChecked(state);
}

void MythUIButtonList::Init()
{
    if (m_initialized)
        return;

    m_initialized = true;
    if (m_upArrow)
        m_upArrow->SetVisible(m_showScrollArrows);

    if (m_downArrow)
        m_downArrow->SetVisible(m_showScrollArrows);

    MythUIStateType *buttontemplate = dynamic_cast<MythUIStateType *>
                                                (GetChild("buttonitem"));

    if (!buttontemplate)
    {
        VERBOSE(VB_IMPORTANT, QString("Statetype buttonitem is required in "
                                      "mythuibuttonlist: %1")
                                      .arg(objectName()));
        return;
    }

    buttontemplate->SetVisible(false);
    QRect buttonItemArea = buttontemplate->GetArea();

    if (buttonItemArea.height() > 0)
        m_itemHeight = buttonItemArea.height();

    if (buttonItemArea.width() > 0)
        m_itemWidth = buttonItemArea.width();
    else
        m_itemWidth = m_contentsRect.width();

//     QFontMetrics fm(m_fontActive->face());
//     QSize sz1 = fm.size(Qt::TextSingleLine, "XXXXX");
//     fm = QFontMetrics(m_fontInactive->face());
//     QSize sz2 = fm.size(Qt::TextSingleLine, "XXXXX");
//    m_itemHeight = QMAX(sz1.height(), sz2.height()) + (int)(2 * m_itemMarginY);
//    m_itemWidth = m_contentsRect.width();

    CalculateVisibleItems();

    int col = 1;
    int row = 1;

    for (int i = 0; i < (int)m_itemsVisible; i++)
    {
        QString name = QString("buttonlist button %1").arg(i);
        MythUIStateType *button = new MythUIStateType(this, name);
        button->CopyFrom(buttontemplate);

        if (col > m_columns)
        {
            col = 1;
            row++;
        }

        button->SetPosition(GetButtonPosition(col, row));

        col++;

        m_ButtonList.push_back(button);
    }

    SetPositionArrowStates();
}

bool MythUIButtonList::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled = false;
    GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
                MoveUp(MoveRow);
            else
                handled = false;
        }
        else if (action == "DOWN")
        {
            if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
                MoveDown(MoveRow);
            else
                handled = false;
        }
        else if (action == "RIGHT")
        {
            if (m_layout == LayoutHorizontal)
                MoveDown(MoveItem);
            else if (m_layout == LayoutGrid)
                MoveDown(MoveColumn);
            else
                handled = false;
        }
        else if (action == "LEFT")
        {
            if (m_layout == LayoutHorizontal)
                MoveUp(MoveItem);
            else if (m_layout == LayoutGrid)
                MoveUp(MoveColumn);
            else
                handled = false;
        }
        else if (action == "PAGEUP")
        {
            MoveUp(MovePage);
        }
        else if (action == "PAGEDOWN")
        {
            MoveDown(MovePage);
        }
        else if (action == "SELECT")
        {
            MythUIButtonListItem *item = GetItemCurrent();
            if (item)
            {
                emit itemSelected(item);
                emit itemClicked(item);
            }
        }
        else
            handled = false;
    }

    return handled;
}

/** \brief Mouse click/movement handler, recieves mouse gesture events from the
 *         QApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
void MythUIButtonList::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
{
    if (event->gesture() == MythGestureEvent::Click)
    {
        QPoint position = event->GetPosition();

        MythUIType *type = GetChildAtPoint(position);

        if (!type)
            return;

        MythUIStateType *button = dynamic_cast<MythUIStateType *>(type);
        if (button)
        {
            QString buttonname = button->objectName();
            int pos = buttonname.section(' ',2,2).toInt();
            if (pos < m_itemList.size())
            {
                SetItemCurrent(pos);
                //MoveToNamedPosition(button->GetText());
                emit itemClicked(GetItemCurrent());
            }

            return;
        }

        MythUIStateType *arrow = dynamic_cast<MythUIStateType *>(type);
        if (arrow)
        {
            QString name = arrow->objectName();

            if (name == "upscrollarrow")
            {
                MoveUp(MovePage);
            }
            else if (name == "downscrollarrow")
            {
                MoveDown(MovePage);
            }
            return;
        }
    }
}

/** \brief Get the button within the list at a specified coordinates
 *
 *  \param p QPoint coordinates
 *
 *  \return MythUIType at these coordinates
 */
MythUIType *MythUIButtonList::GetChildAtPoint(const QPoint &p)
{
    if (GetArea().contains(p))
    {
        if (m_ChildrenList.isEmpty())
            return NULL;

        /* check all children */
        QVector<MythUIType*>::iterator it;
        for (it = m_ChildrenList.end()-1; it != m_ChildrenList.begin()-1; it--)
        {
            MythUIType *child = NULL;

            if ((*it)->GetArea().contains(p - GetArea().topLeft()))
                child = *it;

            if (child != NULL)
                return child;
        }
    }
    return NULL;
}

QPoint MythUIButtonList::GetButtonPosition(int column, int row) const
{
    int x = m_contentsRect.x() +
                            ((column - 1) * (ItemWidth() + m_itemHorizSpacing));
    int y = m_contentsRect.y() +
                            ((row - 1) * (m_itemHeight + m_itemVertSpacing));

    return QPoint(x,y);
}

void MythUIButtonList::CalculateVisibleItems(void)
{
    int y = 0;
    int x = 0;
    m_itemsVisible = 0;
    m_rows = 0;
    m_columns = 0;

    if ((m_layout == LayoutHorizontal) || (m_layout == LayoutGrid))
    {
        while (x <= m_contentsRect.width() - m_itemWidth)
        {
            x += m_itemWidth + m_itemHorizSpacing;
            m_columns++;
        }
    }

    if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
    {
        while (y <= m_contentsRect.height() - m_itemHeight)
        {
            y += m_itemHeight + m_itemVertSpacing;
            m_rows++;
        }
    }

    if (m_rows <= 0)
        m_rows = 1;

    if (m_columns <= 0)
        m_columns = 1;

    m_itemsVisible = m_columns * m_rows;
}

bool MythUIButtonList::ParseElement(QDomElement &element)
{
    if (element.tagName() == "buttonarea")
        m_contentsRect = parseRect(element);
    else if (element.tagName() == "layout")
    {
        QString layout = getFirstText(element).toLower();

        if (layout == "grid")
            m_layout = LayoutGrid;
        else if (layout == "horizontal")
            m_layout = LayoutHorizontal;
        else
            m_layout = LayoutVertical;
    }
    else if (element.tagName() == "scrollstyle")
    {
        QString layout = getFirstText(element).toLower();

        if (layout == "center")
            m_scrollStyle = ScrollCenter;
        else if (layout == "free")
            m_scrollStyle = ScrollFree;
    }
    else if (element.tagName() == "scrollarrows")
    {
        m_showScrollArrows = parseBool(element.attribute("show", ""));
        if (m_showScrollArrows)
        {
            if (m_upArrow)
                delete m_upArrow;
            if (m_downArrow)
                delete m_downArrow;
            ParseChildren(element, this);
            m_upArrow = dynamic_cast<MythUIStateType *>(GetChild("upscrollarrow"));
            m_downArrow = dynamic_cast<MythUIStateType *>(GetChild("downscrollarrow"));
        }
    }
    else if (element.tagName() == "showarrow")
        m_showArrow = parseBool(element);
    else if (element.tagName() == "spacing")
    {
        m_itemHorizSpacing = NormX(getFirstText(element).toInt());
        m_itemVertSpacing = NormY(getFirstText(element).toInt());
    }
    else if (element.tagName() == "drawfrombottom")
        m_drawFromBottom = parseBool(element);
    else
        return MythUIType::ParseElement(element);

    return true;
}

void MythUIButtonList::CreateCopy(MythUIType *parent)
{
    MythUIButtonList *lb = new MythUIButtonList(parent, objectName());
    lb->CopyFrom(this);
}

void MythUIButtonList::CopyFrom(MythUIType *base)
{
    MythUIButtonList *lb = dynamic_cast<MythUIButtonList *>(base);
    if (!lb)
        return;

    m_layout = lb->m_layout;

    m_contentsRect = lb->m_contentsRect;

    m_itemHeight = lb->m_itemHeight;
    m_itemWidth = lb->m_itemWidth;
    m_itemHorizSpacing = lb->m_itemHorizSpacing;
    m_itemVertSpacing = lb->m_itemVertSpacing;
    m_itemsVisible = lb->m_itemsVisible;

    m_active = lb->m_active;
    m_showScrollArrows = lb->m_showScrollArrows;
    m_showArrow = lb->m_showArrow;

    m_drawoffset = lb->m_drawoffset;
    m_drawFromBottom = lb->m_drawFromBottom;

    m_scrollStyle = lb->m_scrollStyle;

    m_clearing = false;
    m_topItem = m_selItem = NULL;

    m_selPosition = m_topPosition = m_itemCount = 0;

    MythUIType::CopyFrom(base);

    m_upArrow = dynamic_cast<MythUIStateType *>(GetChild("upscrollarrow"));
    m_downArrow = dynamic_cast<MythUIStateType *>(GetChild("downscrollarrow"));

    for (int i = 0; i < (int)m_itemsVisible; i++)
    {
        MythUIType *deltype;
        QString name = QString("buttonlist button %1").arg(i);
        if ((deltype = GetChild(name)))
            delete deltype;
    }

    m_ButtonList.clear();

    m_initialized = false;
}

void MythUIButtonList::Finalize(void)
{
    MythUIType::Finalize();
}

//////////////////////////////////////////////////////////////////////////////

MythUIButtonListItem::MythUIButtonListItem(MythUIButtonList* lbtype,
                                       const QString& text,
                                       MythImage *image, bool checkable,
                                       CheckState state, bool showArrow)
{
    if (!lbtype)
        cerr << "Cannot add a button to a non-existant list!";

    m_parent    = lbtype;
    m_text      = text;
    m_image     = image;
    m_checkable = checkable;
    m_state     = state;
    m_showArrow = showArrow;
    m_data      = 0;
    m_overrideInactive = false;

    if (state >= NotChecked)
        m_checkable = true;

    m_parent->InsertItem(this);
}

MythUIButtonListItem::~MythUIButtonListItem()
{
    if (m_parent)
        m_parent->RemoveItem(this);
    if (m_image)
        m_image->DownRef();
}

QString MythUIButtonListItem::text() const
{
    return m_text;
}

void MythUIButtonListItem::setText(const QString &text, const QString &name)
{
    if (!name.isEmpty())
    {
        m_strings.insert(name, text);
    }
    else
        m_text = text;
}

const MythImage* MythUIButtonListItem::image() const
{
    return m_image;
}

void MythUIButtonListItem::setImage(MythImage *image)
{
    if (!image)
        return;
    m_image = image;
    m_image->UpRef();
    m_parent->Update();
}

bool MythUIButtonListItem::checkable() const
{
    return m_checkable;
}

MythUIButtonListItem::CheckState MythUIButtonListItem::state() const
{
    return m_state;
}

MythUIButtonList* MythUIButtonListItem::parent() const
{
    return m_parent;
}

void MythUIButtonListItem::setChecked(MythUIButtonListItem::CheckState state)
{
    if (!m_checkable)
        return;
    m_state = state;
    m_parent->Update();
}

void MythUIButtonListItem::setCheckable(bool flag)
{
    m_checkable = flag;
}

void MythUIButtonListItem::setDrawArrow(bool flag)
{
    m_showArrow = flag;
}

void MythUIButtonListItem::setData(void *data)
{
    m_data = data;
}

void *MythUIButtonListItem::getData()
{
    return m_data;
}

void MythUIButtonListItem::setOverrideInactive(bool flag)
{
    m_overrideInactive = flag;
}

bool MythUIButtonListItem::getOverrideInactive(void)
{
    return m_overrideInactive;
}

bool MythUIButtonListItem::moveUpDown(bool flag)
{
    return m_parent->MoveItemUpDown(this, flag);
}

void MythUIButtonListItem::SetToRealButton(MythUIStateType *button, bool active_on)
{
    if (this == m_parent->m_selItem)
    {
        if (m_parent->m_active && !m_overrideInactive && active_on)
            button->DisplayState("Selected");
        else
        {
//             if (active_on)
//                 button->DisplayState("SelectedInactive");
//             else
                button->DisplayState("Active");
        }
    }
    else
        button->DisplayState("Active");

    MythUIGroup *buttonstate = dynamic_cast<MythUIGroup *>
                                            (button->GetCurrentState());

    if (!buttonstate)
        return;

    buttonstate->SetVisible(true);

    MythUIImage *buttonbackground = dynamic_cast<MythUIImage *>
                                        (buttonstate->GetChild("buttonbackground"));
    if (buttonbackground && buttonbackground->IsGradient())
        buttonbackground->ForceSize(buttonstate->GetArea().size());

    MythUIText *buttontext = dynamic_cast<MythUIText *>
                                        (buttonstate->GetChild("buttontext"));
    if (buttontext)
        buttontext->SetText(m_text);

    MythUIImage *buttonimage = dynamic_cast<MythUIImage *>
                                        (buttonstate->GetChild("buttonimage"));
    if (buttonimage)
        buttonimage->SetImage(m_image);

    MythUIImage *buttonarrow = dynamic_cast<MythUIImage *>
                                        (buttonstate->GetChild("buttonarrow"));
    if (buttonarrow)
        buttonarrow->SetVisible(m_showArrow);

    MythUIStateType *buttoncheck = dynamic_cast<MythUIStateType *>
                                        (buttonstate->GetChild("buttoncheck"));
    if (buttoncheck)
    {
        buttoncheck->SetVisible(m_checkable);

        if (m_checkable)
        {
            if (m_state == NotChecked)
                buttoncheck->DisplayState(MythUIStateType::Off);
            else if (m_state == HalfChecked)
                buttoncheck->DisplayState(MythUIStateType::Half);
            else
                buttoncheck->DisplayState(MythUIStateType::Full);
        }
    }

    MythUIText *text;
    QMap<QString, QString>::iterator string_it = m_strings.begin();
    while (string_it != m_strings.end()) {
        text = dynamic_cast<MythUIText *>
                                        (buttonstate->GetChild(string_it.key()));
        if (text)
            text->SetText(string_it.value());
        ++string_it;
    }

    MythUIImage *image;
    QMap<QString, MythImage*>::iterator image_it = m_images.begin();
    while (image_it != m_images.end()) {
        image = dynamic_cast<MythUIImage *>
                                        (buttonstate->GetChild(image_it.key()));
        if (image)
            image->SetImage(image_it.value());
        ++image_it;
    }
}

