
#include "mythuibuttonlist.h"

#include <math.h>

// QT headers
#include <QDomDocument>

// libmyth headers
#include "mythverbose.h"

// mythui headers
#include "mythuigroup.h"
#include "mythmainwindow.h"
#include "mythuistatetype.h"
#include "lcddevice.h"

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
    m_Area      = area;
    m_showArrow = showArrow;

    Const();
}

void MythUIButtonList::Const(void)
{
    m_contentsRect = MythRect(0,0,0,0);

    m_layout      = LayoutVertical;
    m_scrollStyle = ScrollFree;
    m_wrapStyle   = WrapNone;

    m_active         = false;
    m_drawFromBottom = false;

    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount = 0;
    m_keepSelAtBottom = false;

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
    m_lcdTitle         = "";

    m_upArrow = m_downArrow = NULL;

    SetCanTakeFocus(true);

    connect(this, SIGNAL(TakingFocus()), this, SLOT(Select()));
    connect(this, SIGNAL(LosingFocus()), this, SLOT(Deselect()));
}

MythUIButtonList::~MythUIButtonList()
{
    m_ButtonToItem.clear();
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

void MythUIButtonList::SetDrawFromBottom(bool draw)
{
    m_drawFromBottom = draw;
}

void MythUIButtonList::SetActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
    if (m_initialized)
        Update();
}

void MythUIButtonList::Reset()
{
    m_ButtonToItem.clear();

    if (m_itemList.isEmpty())
        return;

    m_clearing = true;

    while (!m_itemList.isEmpty())
        delete m_itemList.takeFirst();

    m_clearing = false;

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

void MythUIButtonList::SanitizePosition(void)
{
    if (m_selPosition < 0)
        m_selPosition = (m_wrapStyle > WrapNone) ? m_itemList.size() - 1 : 0;
    else if (m_selPosition >= m_itemList.size())
        m_selPosition = (m_wrapStyle > WrapNone) ? 0 : m_itemList.size() - 1;
}

void MythUIButtonList::SetPositionArrowStates(void)
{
    if (!m_initialized)
        Init();

    if (m_clearing)
        return;

    m_needsUpdate = false;

    m_ButtonToItem.clear();

    if (m_ButtonList.size() > 0)
    {
        int button = 0;

        // set topitem, top position
        SanitizePosition();

        switch (m_scrollStyle)
        {
            case ScrollCenter:
                m_topPosition = qMax(m_selPosition - (int)((float)m_itemsVisible / 2), 0);
                break;
            case ScrollFree:
            {
                int adjust = 0;
                if (m_topPosition == -1 || m_keepSelAtBottom)
                {
                    if (m_topPosition == -1)
                        m_topPosition = 0;
                    if (m_layout == LayoutHorizontal)
                        adjust = 1 - m_itemsVisible;
                    else
                        adjust = m_columns - m_itemsVisible;
                    m_keepSelAtBottom = false;
                }

                if (m_selPosition < m_topPosition ||
                    m_topPosition + (int)m_itemsVisible <= m_selPosition)
                {
                    if (m_layout == LayoutHorizontal)
                        m_topPosition = m_selPosition + adjust;
                    else
                        m_topPosition = (m_selPosition + adjust) /
                                        m_columns * m_columns;
                }

                m_topPosition = qMax(m_topPosition, 0);
                break;
            }
        }

        QList<MythUIButtonListItem*>::iterator it = m_itemList.begin() + m_topPosition;

        if (m_scrollStyle == ScrollCenter)
        {
            if (m_selPosition <= (int)(m_itemsVisible/2))
            {
                button = (m_itemsVisible / 2) - m_selPosition;
                if (m_wrapStyle == WrapItems && button > 0 &&
                    m_itemCount >= (int)m_itemsVisible)
                {
                    it = m_itemList.end() - button;
                    button = 0;
                }
            }
            else if ((m_itemCount - m_selPosition) < (int)(m_itemsVisible/2))
            {
                it = m_itemList.begin() + m_selPosition - (m_itemsVisible/2);
            }
        }
        else if (m_drawFromBottom && m_itemCount < (int)m_itemsVisible)
            button = m_itemsVisible - m_itemCount;

        for (int i = 0; i < button; i++)
             m_ButtonList[i]->SetVisible(false);

        bool seenSelected = false;

        MythUIStateType *realButton = NULL;
        MythUIButtonListItem *buttonItem = NULL;

        if (it < m_itemList.begin())
            it = m_itemList.begin();

        int curItem = GetItemPos(*it);
        while (it < m_itemList.end() && button < (int)m_itemsVisible)
        {
            realButton = m_ButtonList[button];
            buttonItem = *it;

            if (!realButton || !buttonItem)
                break;

            bool selected = false;
            if (!seenSelected && (curItem == m_selPosition))
            {
                seenSelected = true;
                selected = true;
            }

            m_ButtonToItem[button] = buttonItem;
            buttonItem->SetToRealButton(realButton, selected);
            realButton->SetVisible(true);

            if (m_wrapStyle == WrapItems && it == (m_itemList.end()-1) &&
                m_itemCount >= (int)m_itemsVisible)
            {
                it = m_itemList.begin();
                curItem = 0;
            }
            else
            {
                ++it;
                ++curItem;
            }

            button++;
        }

        for (; button < (int)m_itemsVisible; button++)
            m_ButtonList[button]->SetVisible(false);

    }

    updateLCD();

    if (!m_downArrow || !m_upArrow)
        return;

    if (m_itemCount == 0)
    {
        m_downArrow->DisplayState(MythUIStateType::Off);
        m_upArrow->DisplayState(MythUIStateType::Off);
    }
    else
    {
        if (m_topPosition != 0)
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

    if (curIndex == m_topPosition &&
        m_topPosition > 0 &&
        m_topPosition == m_itemCount - 1)
    {
        m_topPosition--;
    }

    if (curIndex == m_selPosition &&
        m_selPosition > 0 &&
        m_selPosition == m_itemCount - 1)
    {
        m_selPosition--;
    }

    m_itemList.removeAt(curIndex);
    m_itemCount--;

    Update();

    if (m_selPosition < m_itemCount)
        emit itemSelected(m_itemList.at(m_selPosition));
    else
        emit itemSelected(NULL);
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
    int newIndex = m_itemList.indexOf(item);
    SetItemCurrent(newIndex);
}

void MythUIButtonList::SetItemCurrent(int current, int topPosition)
{
    if (!m_initialized)
        Init();

    if (current == -1 || current >= m_itemList.size())
        return;

    if (current == m_selPosition &&
        (topPosition == -1 || topPosition == m_topPosition))
        return;

    m_topPosition = topPosition;
    if (topPosition > 0 && m_layout == LayoutGrid)
        m_topPosition -= (topPosition % m_columns);

    m_selPosition = current;
    if (m_itemsVisible == 0)
        return;

    Update();

    emit itemSelected(GetItemCurrent());
}

MythUIButtonListItem* MythUIButtonList::GetItemCurrent() const
{
    if (m_itemList.isEmpty() || m_selPosition > m_itemList.size() ||
        m_selPosition < 0)
        return NULL;

    return m_itemList.at(m_selPosition);
}

int MythUIButtonList::GetIntValue() const
{
    MythUIButtonListItem *item = GetItemCurrent();
    if (item)
        return item->GetText().toInt();

    return 0;
}

QString MythUIButtonList::GetValue() const
{
    MythUIButtonListItem *item = GetItemCurrent();
    if (item)
        return item->GetText();

    return QString();
}

QVariant MythUIButtonList::GetDataValue() const
{
    MythUIButtonListItem *item = GetItemCurrent();
    if (item)
        return item->GetData();

    return QVariant();
}

MythUIButtonListItem* MythUIButtonList::GetItemFirst() const
{
    if (!m_itemList.empty())
        return m_itemList[0];
    return NULL;
}

MythUIButtonListItem* MythUIButtonList::GetItemNext(MythUIButtonListItem *item)
const
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

MythUIButtonListItem* MythUIButtonList::GetItemByData(QVariant data)
{
    if (!m_initialized)
        Init();

    for (int i = 0; i < m_itemList.size(); ++i)
    {
        MythUIButtonListItem *item = m_itemList.at(i);
        if (item->GetData() == data)
            return item;
    }

    return NULL;
}

int MythUIButtonList::GetItemPos(MythUIButtonListItem* item) const
{
    if (!item)
        return -1;
    return m_itemList.indexOf(item);
}

bool MythUIButtonList::MoveUp(MovementUnit unit, uint amount)
{
    int pos = m_selPosition;
    if (pos == -1 || m_itemList.isEmpty() || !m_initialized)
        return false;

    switch (unit)
    {
        case MoveItem:
            if (m_selPosition > 0)
                --m_selPosition;
            else if (m_wrapStyle > WrapNone)
                m_selPosition = m_itemList.size() - 1;
            else if (m_wrapStyle == WrapCaptive)
                return true;
            break;
        case MoveColumn:
            if (pos % m_columns > 0)
                --m_selPosition;
            else if (m_wrapStyle > WrapNone)
                m_selPosition = pos + (m_columns-1);
            else if (m_wrapStyle == WrapCaptive)
                return true;
            break;
        case MoveRow:
            if ((pos - m_columns) >= 0)
            {
                for (int i = 0; i < m_columns; ++i)
                {
                    --m_selPosition;
                }
            }
            else if (m_wrapStyle > WrapNone)
            {
                m_selPosition = ((m_itemList.size()-1) / m_columns) *
                                    m_columns + pos;
                if ((m_selPosition / m_columns)
                        < ((m_itemList.size()-1)/ m_columns))
                    m_selPosition = m_itemList.size() - 1;

                if (m_layout == LayoutVertical)
                    m_topPosition = qMax(0, m_selPosition - (int)m_itemsVisible + 1);
            }
            else if (m_wrapStyle == WrapCaptive)
                return true;
            break;
        case MovePage:
            m_selPosition = qMax(0, m_selPosition - (int)m_itemsVisible);
            break;
        case MoveMid:
            m_selPosition = (int)(m_itemList.size() / 2);
            break;
        case MoveMax:
            m_selPosition = 0;
            break;
        case MoveByAmount:
            for (uint i = 0; i < amount; ++i)
            {
                if (m_selPosition > 0)
                    --m_selPosition;
                else if (m_wrapStyle > WrapNone)
                    m_selPosition = m_itemList.size() - 1;
            }
            break;
    }

    SanitizePosition();

    if (pos != m_selPosition)
    {
        Update();
        emit itemSelected(GetItemCurrent());
    }
    else
        return false;

    return true;
}

bool MythUIButtonList::MoveDown(MovementUnit unit, uint amount)
{
    int pos = m_selPosition;
    if (pos == -1 || m_itemList.isEmpty() || !m_initialized)
        return false;

    switch (unit)
    {
        case MoveItem:
            if (m_selPosition < m_itemList.size() - 1)
                ++m_selPosition;
            else if (m_wrapStyle > WrapNone)
                m_selPosition = 0;
            else if (m_wrapStyle == WrapCaptive)
                return true;
            break;
        case MoveColumn:
            if ((pos+1) % m_columns > 0)
                ++m_selPosition;
            else if (m_wrapStyle > WrapNone)
                m_selPosition = pos - (m_columns-1);
            else if (m_wrapStyle == WrapCaptive)
                return true;
            break;
        case MoveRow:
            if (((m_itemList.size()-1) / m_columns) > (pos / m_columns))
            {
                for (int i = 0; i < m_columns; ++i)
                {
                    if (m_selPosition == m_itemList.size()-1)
                        break;
                    ++m_selPosition;
                }
            }
            else if (m_wrapStyle > WrapNone)
                m_selPosition = (pos % m_columns);
            else if (m_wrapStyle == WrapCaptive)
                return true;
            break;
        case MovePage:
            m_selPosition = qMin(m_itemCount - 1,
                                     m_selPosition + (int)m_itemsVisible);
            break;
        case MoveMax:
            m_selPosition = m_itemCount - 1;
            break;
        case MoveByAmount:
            for (uint i = 0; i < amount; ++i)
            {
                if (m_selPosition < m_itemList.size() - 1)
                    ++m_selPosition;
                else if (m_wrapStyle > WrapNone)
                    m_selPosition = 0;
            }
            break;
    }

    SanitizePosition();

    if (pos != m_selPosition)
    {
        m_keepSelAtBottom = true;
        Update();
        emit itemSelected(GetItemCurrent());
    }
    else
        return false;

    return true;
}

bool MythUIButtonList::MoveToNamedPosition(const QString &position_name)
{
    if (!m_initialized)
        Init();

    if (m_selPosition < 0 || m_itemList.isEmpty() || !m_initialized)
        return false;

    bool found_it = false;
    int selectedPosition = 0;
    QList<MythUIButtonListItem*>::iterator it = m_itemList.begin();
    while(it != m_itemList.end())
    {
        if ((*it)->GetText() == position_name)
        {
            found_it = true;
            break;
        }
        ++it;
        ++selectedPosition;
    }

    if (!found_it || m_selPosition == selectedPosition)
        return false;

    SetItemCurrent(selectedPosition);
    return true;
}

bool MythUIButtonList::MoveItemUpDown(MythUIButtonListItem *item, bool up)
{
    if (GetItemCurrent() != item)
        return false;
    if (item == m_itemList.first() && up)
        return false;
    if (item == m_itemList.last() && !up)
        return false;

    int oldpos = m_selPosition;
    int insertat = 0;
    bool dolast = false;

    if (up)
    {
        insertat = m_selPosition - 1;
        if (item == m_itemList.last())
            dolast = true;
        else
            ++m_selPosition;

        if (item == m_itemList.at(m_topPosition))
            ++m_topPosition;
    }
    else
        insertat = m_selPosition + 1;

    m_itemList.removeAt(oldpos);
    m_itemList.insert(insertat, item);

    if (up)
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

    m_initialized = true;
}

uint MythUIButtonList::ItemWidth(void)
{
    if (!m_initialized)
        Init();
    return m_itemWidth;
}

uint MythUIButtonList::ItemHeight(void)
{
    if (!m_initialized)
        Init();
    return m_itemHeight;
}

bool MythUIButtonList::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "UP")
        {
            if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
                handled = MoveUp(MoveRow);
            else
                handled = false;
        }
        else if (action == "DOWN")
        {
            if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
                handled = MoveDown(MoveRow);
            else
                handled = false;
        }
        else if (action == "RIGHT")
        {
            if (m_layout == LayoutHorizontal)
                handled = MoveDown(MoveItem);
            else if (m_layout == LayoutGrid)
                handled = MoveDown(MoveColumn);
            else
                handled = false;
        }
        else if (action == "LEFT")
        {
            if (m_layout == LayoutHorizontal)
                handled = MoveUp(MoveItem);
            else if (m_layout == LayoutGrid)
                handled = MoveUp(MoveColumn);
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
        else if (action == "PAGETOP")
        {
            MoveUp(MoveMax);
        }
        else if (action == "PAGEMIDDLE")
        {
            MoveUp(MoveMid);
        }
        else if (action == "PAGEBOTTOM")
        {
            MoveDown(MoveMax);
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

/** \brief Mouse click/movement handler, receives mouse gesture events from the
 *         QApplication event loop. Should not be used directly.
 *
 *  \param uitype The mythuitype receiving the event
 *  \param event Mouse event
 */
void MythUIButtonList::gestureEvent(MythUIType *uitype, MythGestureEvent *event)
{
    if (event->gesture() == MythGestureEvent::Click)
    {
        // We want the relative position of the click
        QPoint position = event->GetPosition() - m_Parent->GetArea().topLeft();
        
        MythUIType *type = uitype->GetChildAt(position,false,false);

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
                MythUIButtonListItem *item = m_ButtonToItem[pos];
                if (item)
                {
                    if (item == GetItemCurrent())
                        emit itemClicked(item);
                    else
                        SetItemCurrent(item);
                }
            }

            return;
        }
    }
}

QPoint MythUIButtonList::GetButtonPosition(int column, int row) const
{
    int x = m_contentsRect.x() +
                            ((column - 1) * (m_itemWidth + m_itemHorizSpacing));
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
    else if (element.tagName() == "wrapstyle")
    {
        QString wrapstyle = getFirstText(element).toLower();

        if (wrapstyle == "captive")
            m_wrapStyle = WrapCaptive;
        else if (wrapstyle == "none")
            m_wrapStyle = WrapNone;
        else if (wrapstyle == "selection")
            m_wrapStyle = WrapSelect;
        else if (wrapstyle == "items")
            m_wrapStyle = WrapItems;
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
    m_wrapStyle = lb->m_wrapStyle;

    m_clearing = false;
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

void MythUIButtonList::SetLCDTitles(const QString &title, const QString &columnList)
{
    m_lcdTitle = title;
    m_lcdColumns = columnList.split('|');
}

void MythUIButtonList::updateLCD(void)
{
    if (!m_HasFocus)
        return;

    LCD *lcddev = LCD::Get();
    if (lcddev == NULL)
        return;

    // Build a list of the menu items
    QList<LCDMenuItem> menuItems;
    bool selected;

    int start = std::max(0, (int)(m_selPosition - lcddev->getLCDHeight()));
    int end = std::min(m_itemCount, (int)(start + (lcddev->getLCDHeight() * 2)));

    for (int r = start; r < end; r++)
    {
        if (r == GetCurrentPos())
            selected = true;
        else
            selected = false;

        MythUIButtonListItem *item = GetItemAt(r);
        CHECKED_STATE state = NOTCHECKABLE;
        if (item->checkable())
            state = (item->state() == MythUIButtonListItem::NotChecked) ? UNCHECKED : CHECKED;

        QString text;
        for (int x = 0; x < m_lcdColumns.count(); x++)
        {
            if (!m_lcdColumns[x].isEmpty() && item->m_strings.contains(m_lcdColumns[x]))
            {
                // named text column
                MythUIButtonListItem::TextProperties props = item->m_strings[m_lcdColumns[x]];
                if (text.isEmpty())
                    text = props.text;
                else
                    text += " ~ " + props.text;
            }
            else
            {
                // default text column
                if (text.isEmpty())
                    text = item->GetText();
                else
                    text += " ~ " + item->GetText();
            }
        }

        if (!text.isEmpty())
            menuItems.append(LCDMenuItem(selected, state, text));
        else
            menuItems.append(LCDMenuItem(selected, state, item->GetText()));
    }

    if (!menuItems.isEmpty())
        lcddev->switchToMenu(menuItems, m_lcdTitle);
}

//////////////////////////////////////////////////////////////////////////////

MythUIButtonListItem::MythUIButtonListItem(MythUIButtonList* lbtype,
                                      const QString& text, const QString& image,
                                      bool checkable, CheckState state,
                                      bool showArrow)
{
    if (!lbtype)
        VERBOSE(VB_IMPORTANT, "Cannot add a button to a non-existent list!");

    m_parent    = lbtype;
    m_text      = text;
    m_image     = NULL;
    m_imageFilename = image;
    m_checkable = checkable;
    m_state     = state;
    m_showArrow = showArrow;
    m_data      = 0;

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
        VERBOSE(VB_IMPORTANT, "Cannot add a button to a non-existent list!");

    m_parent    = lbtype;
    m_text      = text;
    m_data      = data;

    m_image     = NULL;

    m_checkable = false;
    m_state     = CantCheck;
    m_showArrow = false;

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
    while (it.hasNext())
    {
        it.next();
        it.value()->DownRef();
    }
}

void MythUIButtonListItem::SetText(const QString &text, const QString &name,
                                   const QString &state)
{
    if (!name.isEmpty())
    {
        TextProperties textprop;
        textprop.text = text;
        textprop.state = state;
        m_strings.insert(name, textprop);
    }
    else
        m_text = text;

    if (m_parent)
        m_parent->Update();
}

void MythUIButtonListItem::SetTextFromMap(QHash<QString, QString> &infoMap,
                                   const QString &state)
{
    QHash<QString, QString>::iterator map_it = infoMap.begin();
    while (map_it != infoMap.end())
    {
        TextProperties textprop;
        textprop.text = (*map_it);
        textprop.state = state;
        m_strings[map_it.key()] = textprop;
        ++map_it;
    }

    if (m_parent)
        m_parent->Update();
}

QString MythUIButtonListItem::GetText() const
{
    return m_text;
}

void MythUIButtonListItem::SetFontState(const QString &state,
                                        const QString &name)
{
    if (!name.isEmpty())
    {
        if (m_strings.contains(name))
            m_strings[name].state = state;
    }
    else
        m_fontState = state;

    if (m_parent)
        m_parent->Update();
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

MythImage* MythUIButtonListItem::getImage(const QString &name)
{
    if (!name.isEmpty())
    {
        if (m_images.contains(name))
            return m_images.value(name);
    }
    else
    {
        return m_image;
    }

    return NULL;
}

void MythUIButtonListItem::SetImage(
    const QString &filename, const QString &name, bool force_reload)
{
    bool do_update = force_reload;
    if (!name.isEmpty())
    {
        QMap<QString, QString>::iterator it = m_imageFilenames.find(name);
        if (it == m_imageFilenames.end())
        {
            m_imageFilenames.insert(name, filename);
            do_update = true;
        }
        else if (*it != filename)
        {
            *it = filename;
            do_update = true;
        }
    }
    else if (m_imageFilename != filename)
    {
        m_imageFilename = filename;
        do_update = true;
    }

    if (m_parent && do_update)
        m_parent->Update();
}

QString MythUIButtonListItem::GetImage(const QString &name) const
{
    if (name.isEmpty())
        return m_imageFilename;

    QMap<QString, QString>::const_iterator it = m_imageFilenames.find(name);
    if (it != m_imageFilenames.end())
        return *it;

    return QString();
}

void MythUIButtonListItem::DisplayState(const QString &state,
                                        const QString &name)
{
    if (name.isEmpty())
        return;

    bool do_update = false;
    QMap<QString, QString>::iterator it = m_states.find(name);
    if (it == m_states.end())
    {
        m_states.insert(name, state);
        do_update = true;
    }
    else if (*it != state)
    {
        *it = state;
        do_update = true;
    }

    if (m_parent && do_update)
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
    if (!m_checkable || m_state == state)
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

void MythUIButtonListItem::SetData(QVariant data)
{
    m_data = data;
}

QVariant MythUIButtonListItem::GetData()
{
    return m_data;
}

bool MythUIButtonListItem::MoveUpDown(bool flag)
{
    if (m_parent)
        return m_parent->MoveItemUpDown(this, flag);
    else
        return false;
}

void MythUIButtonListItem::SetToRealButton(MythUIStateType *button, bool selected)
{
    if (!m_parent)
        return;

    QString state = "active";
    if (selected)
    {
        button->MoveToTop();

        if (m_parent->m_active)
        {
            state = "selected";
        }
        else
            state = "inactive";
    }

    button->DisplayState(state);

    MythUIGroup *buttonstate = dynamic_cast<MythUIGroup *>
                                            (button->GetCurrentState());

    if (!buttonstate)
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to query buttonlist state: %1")
                                                                    .arg(state));
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
    {
        buttontext->SetText(m_text);
        buttontext->SetFontState(m_fontState);
    }

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
    QMap<QString, TextProperties>::iterator string_it = m_strings.begin();
    while (string_it != m_strings.end())
    {
        text = dynamic_cast<MythUIText *>
                                    (buttonstate->GetChild(string_it.key()));
        if (text)
        {
            TextProperties textprop = string_it.value();
            text->SetText(textprop.text);
            text->SetFontState(textprop.state);
        }
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
        {
            if (!statetype->DisplayState(state_it.value()))
                statetype->Reset();
        }
        ++state_it;
    }
}
