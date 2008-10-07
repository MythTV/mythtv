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
#include "mythverbose.h"

#define LOC     QString("MythUIButtonList(%1): ").arg(objectName())
#define LOC_ERR QString("MythUIButtonList(%1), Error: ").arg(objectName())

MythUIButtonList::MythUIButtonList(MythUIType *parent, const QString &name)
              : MythUIType(parent, name)
{
    m_showArrow = true;

    Const();
}

MythUIButtonList::MythUIButtonList(MythUIType *parent, const QString &name,
                               const QRect& area, bool showArrow,
                               bool showScrollArrows)
              : MythUIType(parent, name)
{
    m_Area             = area;

    m_showArrow        = showArrow;

    Const();
}

void MythUIButtonList::Const(void)
{
    m_contentsRect = MythRect(0,0,0,0);

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
    m_needsUpdate      = false;
    m_clearing         = false;
    m_itemHorizSpacing = 0;
    m_itemVertSpacing  = 0;
    m_itemHeight       = 0;
    m_itemWidth        = 0;
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
        Update();
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

    Update();
    MythUIType::Reset();
}

void MythUIButtonList::Update()
{
    m_needsUpdate = true;
    SetRedraw();
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

    if (!m_downArrow || !m_upArrow)
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

    m_needsUpdate = false;
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

    Update();
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

    Update();

    if (m_selItem)
        emit itemSelected(m_selItem);
}

void MythUIButtonList::SetValueByData(QVariant data)
{
    if (!m_initialized)
        Init();

    for (int i = 0; i < m_itemList.size(); ++i)
    {
        MythUIButtonListItem *item = m_itemList.at(i);
        if (item->GetData() == data)
        {
            SetItemCurrent(item);
            return;
        }
    }
}

void MythUIButtonList::SetItemCurrent(MythUIButtonListItem* item)
{
    if (!m_initialized)
        Init();

    m_selPosition = m_itemList.indexOf(item);

    if (m_selPosition == -1)
        m_selPosition = 0;

    m_selItem = m_itemList.at(m_selPosition);

    if (m_itemsVisible == 0)
        return;

    switch (m_scrollStyle)
    {
        case ScrollCenter :
            while (m_topPosition + (int)((float)m_itemsVisible/2) < m_selPosition + 1)
                ++m_topPosition;

            break;
        case ScrollFree :
            while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1)
                m_topPosition += m_columns;

            break;
    }

    m_topItem = m_itemList.at(m_topPosition);

    Update();

    emit itemSelected(m_selItem);
}

void MythUIButtonList::SetItemCurrent(int current)
{
    if (current >= m_itemList.size())
        return;

    MythUIButtonListItem* item = m_itemList.at(current);
    if (!item && !m_itemList.empty())
        item = m_itemList[0];

    if (item)
        SetItemCurrent(item);
}

MythUIButtonListItem* MythUIButtonList::GetItemCurrent() const
{
    return m_selItem;
}

MythUIButtonListItem* MythUIButtonList::GetItemFirst() const
{
    if (!m_itemList.empty())
        return m_itemList[0];
    return NULL;
}

MythUIButtonListItem* MythUIButtonList::GetItemNext(MythUIButtonListItem *item) const
{
    QListIterator<MythUIButtonListItem*> it(m_itemList);
    if (!it.findNext(item))
        return 0;

    return it.previous();
}

int MythUIButtonList::GetCount() const
{
    return m_itemCount;
}

bool MythUIButtonList::IsEmpty() const
{
    if (m_itemCount > 0)
        return false;
    else
        return true;
}

MythUIButtonListItem* MythUIButtonList::GetItemAt(int pos) const
{
    return m_itemList.at(pos);
}

int MythUIButtonList::GetItemPos(MythUIButtonListItem* item) const
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

    Update();

    if (pos != m_selPosition)
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

    Update();

    if (pos != m_selPosition)
        emit itemSelected(m_selItem);
}

bool MythUIButtonList::MoveToNamedPosition(const QString &position_name)
{
    if (!m_initialized)
        Init();

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

    Update();

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

    m_upArrow = dynamic_cast<MythUIStateType *>(GetChild("upscrollarrow"));
    m_downArrow = dynamic_cast<MythUIStateType *>(GetChild("downscrollarrow"));

    if (m_upArrow)
        m_upArrow->SetVisible(true);

    if (m_downArrow)
        m_downArrow->SetVisible(true);

    MythUIStateType *buttontemplate = dynamic_cast<MythUIStateType *>
                                                (GetChild("buttonitem"));

    if (!buttontemplate)
    {
        VERBOSE(VB_IMPORTANT, QString("Statetype buttonitem is required in "
                                      "mythuibuttonlist: %1")
                                      .arg(objectName()));
        return;
    }

    m_contentsRect.CalculateArea(m_Area);

    buttontemplate->SetVisible(false);

    MythRect buttonItemArea;

    MythUIGroup *buttonActiveState = dynamic_cast<MythUIGroup *>
                                        (buttontemplate->GetState("active"));
    if (buttonActiveState)
        buttonItemArea = buttonActiveState->GetArea();
    else
        buttonItemArea = buttontemplate->GetArea();

    buttonItemArea.CalculateArea(m_contentsRect);

    m_itemHeight = buttonItemArea.height();
    m_itemWidth = buttonItemArea.width();

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

    // The following is pretty much a hack for the benefit of MythGallery
    // it scales images based on the button size and we need to give it the
    // largest button state so that the images are not too small
    // This can be removed once the disk based image caching is added to mythui,
    // since the mythgallery thumbnail generator can be ditched.
    MythUIGroup *buttonSelectedState = dynamic_cast<MythUIGroup *>
                                        (buttontemplate->GetState("selected"));

    if (buttonSelectedState)
    {
        MythRect itemArea = buttonSelectedState->GetArea();
        itemArea.CalculateArea(m_contentsRect);

        if (m_itemHeight < itemArea.height())
            m_itemHeight = itemArea.height();
        if (m_itemWidth < itemArea.width())
            m_itemWidth = itemArea.width();
    }
    // End Hack
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
                emit itemClicked(item);
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

        MythUIType *type = GetChildAt(position,false);

        if (!type)
            return;

        MythUIStateType *object = dynamic_cast<MythUIStateType *>(type);
        if (object)
        {
            QString name = object->objectName();

            if (name == "upscrollarrow")
            {
                MoveUp(MovePage);
            }
            else if (name == "downscrollarrow")
            {
                MoveDown(MovePage);
            }
            else if (name.startsWith("buttonlist button"))
            {
                int pos = name.section(' ',2,2).toInt();
                pos += m_topPosition;
                if (pos < m_itemList.size())
                {
                    if (pos == GetCurrentPos())
                    {
                        emit itemClicked(GetItemCurrent());
                    }
                    else if (pos != m_selPosition)
                    {
                        SetItemCurrent(pos);
                        emit itemSelected(GetItemCurrent());
                    }
                }
            }

            return;
        }
    }
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

void MythUIButtonList::DrawSelf(MythPainter *, int, int, int, QRect)
{
    if (m_needsUpdate)
        SetPositionArrowStates();
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
    m_showArrow = lb->m_showArrow;

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
        VERBOSE(VB_IMPORTANT, "Cannot add a button to a non-existant list!");

    m_parent    = lbtype;
    m_text      = text;
    m_image     = image;
    m_imageFilename = "";
    m_checkable = checkable;
    m_state     = state;
    m_showArrow = showArrow;
    m_data      = 0;
    m_overrideInactive = false;

    if (state >= NotChecked)
        m_checkable = true;

    if (m_parent)
        m_parent->InsertItem(this);
}

MythUIButtonListItem::MythUIButtonListItem(MythUIButtonList* lbtype,
                                       const QString& text,
                                       QVariant data)
{
    if (!lbtype)
        VERBOSE(VB_IMPORTANT, "Cannot add a button to a non-existant list!");

    m_parent    = lbtype;
    m_text      = text;
    m_data      = data;

    m_image     = NULL;
    m_imageFilename = "";
    m_checkable = false;
    m_state     = CantCheck;
    m_showArrow = false;
    m_overrideInactive = false;

    if (m_parent)
        m_parent->InsertItem(this);
}

MythUIButtonListItem::~MythUIButtonListItem()
{
    if (m_parent)
        m_parent->RemoveItem(this);

    if (m_image)
        m_image->DownRef();

    QMapIterator<QString, MythImage*> it(m_images);
    while (it.hasNext()) {
        it.next();
        it.value()->DownRef();
    }
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

    if (m_parent)
        m_parent->Update();
}

const QString MythUIButtonListItem::Image() const
{
    return m_imageFilename;
}

void MythUIButtonListItem::setImage(MythImage *image, const QString &name)
{
    if (!name.isEmpty())
    {
        if (m_images.contains(name))
            m_images.value(name)->DownRef();
        m_images.insert(name, image);
        if (image)
            image->UpRef();
    }
    else
    {
        if (image == m_image)
            return;

        if (m_image)
            m_image->DownRef();
        m_image = image;
        if (image)
            image->UpRef();
    }

    if (m_parent)
        m_parent->Update();
}

void MythUIButtonListItem::SetImage(const QString &filename, const QString &name)
{
    if (!name.isEmpty())
        m_imageFilenames.insert(name, filename);
    else
    {
        if (filename == m_imageFilename)
            return;

        m_imageFilename = filename;
    }

    if (m_parent)
        m_parent->Update();
}

void MythUIButtonListItem::DisplayState(const QString &state,
                                        const QString &name)
{
    if (!name.isEmpty())
        m_states.insert(name, state);

    if (m_parent)
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
    if (m_parent)
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
    m_data = qVariantFromValue(data);
}

void *MythUIButtonListItem::getData()
{
    void *vp = m_data.value<void *>();
    return vp;
}

void MythUIButtonListItem::SetData(QVariant data)
{
    m_data = data;
}

QVariant MythUIButtonListItem::GetData()
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
    if (m_parent)
        return m_parent->MoveItemUpDown(this, flag);
    else
        return false;
}

void MythUIButtonListItem::SetToRealButton(MythUIStateType *button, bool active_on)
{
    if (!m_parent)
        return;

    if (this == m_parent->m_selItem)
    {
        if (m_parent->m_active && !m_overrideInactive && active_on)
        {
            button->DisplayState("selected");
            button->MoveToTop();
        }
        else
        {
            if (active_on)
                button->DisplayState("inactive");
            else
                button->DisplayState("active");
        }
    }
    else
        button->DisplayState("active");

    MythUIGroup *buttonstate = dynamic_cast<MythUIGroup *>
                                            (button->GetCurrentState());

    if (!buttonstate)
    {
        VERBOSE(VB_IMPORTANT, "Failed to query button state");
        return;
    }

    buttonstate->SetVisible(true);
    buttonstate->Reset();

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
    {
        if (!m_imageFilename.isEmpty())
        {
            buttonimage->SetFilename(m_imageFilename);
            buttonimage->Load();
        }
        else if (m_image)
            buttonimage->SetImage(m_image);
    }

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
    while (string_it != m_strings.end())
    {
        text = dynamic_cast<MythUIText *>
                                    (buttonstate->GetChild(string_it.key()));
        if (text)
            text->SetText(string_it.value());
        ++string_it;
    }

    MythUIImage *image;
    QMap<QString, QString>::iterator imagefile_it = m_imageFilenames.begin();
    while (imagefile_it != m_imageFilenames.end())
    {
        image = dynamic_cast<MythUIImage *>
                                    (buttonstate->GetChild(imagefile_it.key()));
        if (image)
        {
            image->SetFilename(imagefile_it.value());
            image->Load();
        }
        ++imagefile_it;
    }

    QMap<QString, MythImage*>::iterator image_it = m_images.begin();
    while (image_it != m_images.end())
    {
        image = dynamic_cast<MythUIImage *>
                                    (buttonstate->GetChild(image_it.key()));
        if (image)
            image->SetImage(image_it.value());
        ++image_it;
    }

    MythUIStateType *statetype;
    QMap<QString, QString>::iterator state_it = m_states.begin();
    while (state_it != m_states.end())
    {
        statetype = dynamic_cast<MythUIStateType *>
                                    (buttonstate->GetChild(state_it.key()));
        if (statetype)
            statetype->DisplayState(state_it.value());
        ++state_it;
    }
}

