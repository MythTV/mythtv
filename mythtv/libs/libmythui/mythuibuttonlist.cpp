#include "mythuibuttonlist.h"

#include <cmath>
#include <algorithm>
#include <utility>

// QT headers
#include <QCoreApplication>
#include <QDomDocument>
#include <QKeyEvent>
#include <QRegularExpression>

// libmythbase headers
#include "libmythbase/lcddevice.h"
#include "libmythbase/mythlogging.h"

// mythui headers
#include "mythmainwindow.h"
#include "mythuiscrollbar.h"
#include "mythuistatetype.h"
#include "mythuibutton.h"
#include "mythuitext.h"
#include "mythuitextedit.h"
#include "mythuigroup.h"
#include "mythuiimage.h"
#include "mythgesture.h"
#include "mythuiprogressbar.h"

#define LOC     QString("MythUIButtonList(%1): ").arg(objectName())

MythUIButtonList::MythUIButtonList(MythUIType *parent, const QString &name,
                                   const QString &shadow)
    : MythUIType(parent, name)
    , m_shadowListName(shadow)
{
    // Parent members
    connect(this, &MythUIType::Enabling, this, &MythUIButtonList::ToggleEnabled);
    connect(this, &MythUIType::Disabling, this, &MythUIButtonList::ToggleEnabled);

    Const();
}

MythUIButtonList::MythUIButtonList(MythUIType *parent, const QString &name,
                                   const QRect area, bool showArrow,
                                   bool showScrollBar)
    : MythUIType(parent, name),
      m_showArrow(showArrow), m_showScrollBar(showScrollBar)
{
    // Parent members
    m_area      = area;
    m_initiator = true;
    m_enableInitiator = true;

    connect(this, &MythUIType::Enabling, this, &MythUIButtonList::ToggleEnabled);
    connect(this, &MythUIType::Disabling, this, &MythUIButtonList::ToggleEnabled);

    Const();
}

void MythUIButtonList::Const(void)
{
    SetCanTakeFocus(true);

    connect(this, &MythUIType::TakingFocus, this, &MythUIButtonList::Select);
    connect(this, &MythUIType::LosingFocus, this, &MythUIButtonList::Deselect);
    connect(this, &MythUIType::RequestUpdate, this, &MythUIButtonList::Update);
}

MythUIButtonList::~MythUIButtonList()
{
    m_buttonToItem.clear();
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

void MythUIButtonList::ToggleEnabled()
{
    if (m_initialized)
        Update();
}

#if 0
void MythUIButtonList::SetDrawFromBottom(bool draw)
{
    m_drawFromBottom = draw;
}
#endif

void MythUIButtonList::SetActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;

    if (m_initialized)
        Update();
}

/*!
 * \copydoc MythUIType::Reset()
 */
void MythUIButtonList::Reset()
{
    m_buttonToItem.clear();

    if (m_itemList.isEmpty())
        return;

    m_clearing = true;

    while (!m_itemList.isEmpty())
        delete m_itemList.takeFirst();

    m_clearing = false;

    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount   = 0;

    StopLoad();
    Update();
    MythUIType::Reset();

    emit DependChanged(true);
}

void MythUIButtonList::Update()
{
    m_needsUpdate = true;
    SetRedraw();
}

/*
 * The "width" of a button determines it relative position when using
 * Dynamic-Layout.
 *
 * If a button has a negative offset, that offset needs accounted for
 * to position the button in proper releation to the surrounding buttons.
 */
int MythUIButtonList::minButtonWidth(const MythRect &area)
{
    int width = area.width();

    if (area.x() < 0)
    {
        /*
         * Assume if an overlap is allowed on the left, the same overlap
         * is on the right
         */
        width += ((area.x() * 2) - 1); // x is negative

        while (width < 0)
            width -= area.x(); // Oops
    }
    else if (m_layout == LayoutHorizontal)
    {
        width -= area.x();  // Get rid of any "space" betwen the buttons
    }

    return width;
}

/*
 * The "height" of a button determines it relative position when using
 * Dynamic-Layout.
 *
 * If a button has a negative offset, that offset needs accounted for
 * to position the button in proper releation to the surrounding buttons.
 */
int MythUIButtonList::minButtonHeight(const MythRect &area)
{
    int height = area.height();

    if (area.y() < 0)
    {
        /*
         * Assume if an overlap is allowed on the top, the same overlap
         * is on the bottom
         */
        height += ((area.y() * 2) - 1);

        while (height < 0)
            height -= area.y(); // Oops
    }
    else if (m_layout == LayoutVertical)
    {
        height -= area.y();  // Get rid of any "space" betwen the buttons
    }

    return height;
}

/*
 * For Dynamic-Layout, buttons are allocated as needed.  If the list is
 * being redrawn, re-use any previously allocated buttons.
 */
MythUIGroup *MythUIButtonList::PrepareButton(int buttonIdx, int itemIdx,
                                             int &selectedIdx,
                                             int &button_shift)
{
    MythUIButtonListItem *buttonItem = m_itemList[itemIdx];

    buttonIdx += button_shift;

    if (buttonIdx < 0 || buttonIdx + 1 > m_maxVisible)
    {
        QString name = QString("buttonlist button %1").arg(m_maxVisible);
        auto *button = new MythUIStateType(this, name);
        button->CopyFrom(m_buttontemplate);
        button->ConnectDependants(true);

        if (buttonIdx < 0)
        {
            /*
             * If a new button is needed in the front of the list, previously
             * existing buttons need shifted to the right.
             */
            m_buttonList.prepend(button);
            buttonIdx = 0;
            ++button_shift;

            if (selectedIdx >= 0)
                ++selectedIdx;
        }
        else
        {
            m_buttonList.append(button);
        }

        ++m_maxVisible;
    }

    MythUIStateType *realButton = m_buttonList[buttonIdx];
    m_buttonToItem[buttonIdx] = buttonItem;
    buttonItem->SetToRealButton(realButton, itemIdx == m_selPosition);
    auto *buttonstate =
        dynamic_cast<MythUIGroup *>(realButton->GetCurrentState());

    if (itemIdx == m_selPosition)
        selectedIdx = buttonIdx;

    return buttonstate;
}

/*
 * Dynamically layout the buttons on a row.
 */
bool MythUIButtonList::DistributeRow(int &first_button, int &last_button,
                                     int &first_item, int &last_item,
                                     int &selected_column, int &skip_cols,
                                     bool grow_left, bool grow_right,
                                     int **col_widths, int &row_height,
                                     int total_height, int split_height,
                                     int &col_cnt, bool &wrapped)
{
    MythUIGroup *buttonstate = nullptr;
    int  left_width = 0;
    int  right_width = 0;
    int  begin = 0;
    int  end = 0;
    bool underflow = false;

    int selectedIdx = -1;
    int button_shift = 0;
    col_cnt = 1;
    skip_cols = 0;

    if (last_item + 1 > m_itemCount || last_item < 0 || first_item < 0)
        return false;

    /*
     * Allocate a button on the row.  With a vertical layout, there is
     * only one button per row, and this would be it.
     */
    if (grow_right)
    {
        buttonstate = PrepareButton(last_button, last_item,
                                    selectedIdx, button_shift);
    }
    else
    {
        buttonstate = PrepareButton(first_button, first_item,
                                    selectedIdx, button_shift);
    }

    if (buttonstate == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to query buttonlist state: %1")
            .arg(last_button));
        return false;
    }

    // Note size of initial button.
    int max_width  = m_contentsRect.width();
    int max_height = m_contentsRect.height();
    row_height = minButtonHeight(buttonstate->GetArea());
    int width = minButtonWidth(buttonstate->GetArea());

    /*
     * If the selected button should be centered, don't allow new buttons
     * to take up more than half the allowed area.
     */
    bool vsplit = (m_scrollStyle == ScrollCenter);
    bool hsplit = vsplit && grow_left && grow_right;

    if (hsplit)
    {
        max_width /= 2;
        left_width = right_width = (width / 2);
    }
    else
    {
        if (grow_right)
        {
            left_width  = 0;
            right_width = width;
        }
        else
        {
            left_width  = width;
            right_width = 0;
        }
    }

    if (vsplit)
        max_height /= 2;

    /*
     * If total_height == 0, then this is the first row, so allow any height.
     * Otherwise, If adding a button to a column would exceed the
     * parent height, abort
    */
    if (total_height > 0 &&
        ((vsplit ? split_height : total_height) +
         m_itemVertSpacing + row_height > max_height))
    {
        LOG(VB_GUI, LOG_DEBUG,
            QString("%1 Height exceeded %2 + (%3) + %4 = %5 which is > %6")
            .arg(vsplit ? "Centering" : "Total")
            .arg(split_height).arg(m_itemVertSpacing).arg(row_height)
            .arg(split_height + m_itemVertSpacing + row_height)
            .arg(max_height));
        first_button += button_shift;
        last_button += button_shift;
        return false;
    }

    LOG(VB_GUI, LOG_DEBUG, QString("Added button item %1 width %2 height %3")
        .arg(grow_right ? last_item : first_item)
        .arg(width).arg(row_height));

    int initial_first_button = first_button;
    int initial_last_button  = last_button;
    int initial_first_item   = first_item;
    int initial_last_item    = last_item;

    /*
     * if col_widths is not nullptr, then grow_left & grow_right
     * are mutually exclusive.  So, col_idx can be anchored from
     * the left or right.
    */
    int  col_idx = 0;
    if (!grow_right)
        col_idx = m_columns - 1;

    // Add butons until no more fit.
    bool added = (m_layout != LayoutVertical);

    while (added)
    {
        added = false;

        // If a grid, maintain same number of columns on each row.
        if (grow_right && col_cnt < m_columns)
        {
            if (wrapped)
                end = first_item;
            else
            {
                // Are we allowed to wrap when we run out of items?
                if (m_wrapStyle == WrapItems &&
                    (hsplit || m_scrollStyle != ScrollFree) &&
                    last_item + 1 == m_itemCount)
                {
                    last_item = -1;
                    wrapped = true;
                    end = first_item;
                }
                else
                {
                    end = m_itemCount;
                }
            }

            if (last_item + 1 < end)
            {
                // Allocate next button to the right.
                buttonstate = PrepareButton(last_button + 1, last_item + 1,
                                            selectedIdx, button_shift);

                if (buttonstate == nullptr)
                    continue;

                width = minButtonWidth(buttonstate->GetArea());

                // For grids, use the widest button in a column
                if (*col_widths && width < (*col_widths)[col_idx])
                    width = (*col_widths)[col_idx];

                // Does the button fit?
                if ((hsplit ? right_width : left_width + right_width) +
                    m_itemHorizSpacing + width > max_width)
                {
                    int total = hsplit ? right_width : left_width + right_width;
                    LOG(VB_GUI, LOG_DEBUG,
                        QString("button on right would exceed width: "
                                "%1+(%2)+%3 == %4 which is > %5")
                        .arg(total).arg(m_itemHorizSpacing).arg(width)
                        .arg(total + m_itemHorizSpacing + width)
                        .arg(max_width));
                }
                else
                {
                    added = true;
                    ++col_cnt;
                    ++last_button;
                    ++last_item;
                    ++col_idx;
                    right_width += m_itemHorizSpacing + width;
                    int height = minButtonHeight(buttonstate->GetArea());

                    row_height = std::max(row_height, height);

                    LOG(VB_GUI, LOG_DEBUG,
                        QString("Added button item %1 "
                                "R.width %2 height %3 total width %4+%5"
                                " (max %6)")
                        .arg(last_item).arg(width).arg(height)
                        .arg(left_width).arg(right_width).arg(max_width));
                }
            }
            else
            {
                underflow = true;
            }
        }

        // If a grid, maintain same number of columns on each row.
        if (grow_left && col_cnt < m_columns)
        {
            if (wrapped)
                end = last_item + 1;
            else
            {
                // Are we allowed to wrap when we run out of items?
                if (m_wrapStyle == WrapItems &&
                    (hsplit || m_scrollStyle != ScrollFree) &&
                    first_item == 0)
                {
                    first_item = m_itemCount;
                    wrapped = true;
                    end = last_item + 1;
                }
                else
                {
                    end = 0;
                }
            }

            if (first_item > end)
            {
                buttonstate = PrepareButton(first_button - 1, first_item - 1,
                                            selectedIdx, button_shift);

                if (buttonstate == nullptr)
                    continue;

                width = minButtonWidth(buttonstate->GetArea());

                // For grids, use the widest button in a column
                if (*col_widths && width < (*col_widths)[col_idx])
                    width = (*col_widths)[col_idx];

                // Does the button fit?
                if ((hsplit ? left_width : left_width + right_width) +
                    m_itemHorizSpacing + width > max_width)
                {
                    int total = hsplit ? left_width : left_width + right_width;
                    LOG(VB_GUI, LOG_DEBUG,
                        QString("button on left would exceed width: "
                                "%1+(%2)+%3 == %4 which is > %5")
                        .arg(total).arg(m_itemHorizSpacing).arg(width)
                        .arg(total + m_itemHorizSpacing + width)
                        .arg(max_width));
                }
                else
                {
                    added = true;
                    --first_button;
                    --first_item;
                    --col_idx;
                    ++col_cnt;
                    left_width += m_itemHorizSpacing + width;
                    int height = minButtonHeight(buttonstate->GetArea());

                    row_height = std::max(row_height, height);

                    LOG(VB_GUI, LOG_DEBUG,
                        QString("Added button item %1 "
                                "L.width %2 height %3 total width %4+%5"
                                " (max %6)")
                        .arg(first_item).arg(width).arg(height)
                        .arg(left_width).arg(right_width).arg(max_width));
                }
            }
            else
            {
                underflow = true;
                if (m_layout == LayoutGrid)
                    skip_cols = m_columns - col_cnt;
            }
        }
    }

    /*
     * If total_height == 0, then this is the first row, so allow any height.
     * Otherwise, If adding a button to a column would exceed the
     * parent height, abort
    */
    if (total_height > 0 &&
        ((vsplit ? split_height : total_height) +
         m_itemVertSpacing + row_height > max_height))
    {
        LOG(VB_GUI, LOG_DEBUG,
            QString("%1 Height exceeded %2 + (%3) + %4 = %5 which is > %6")
            .arg(vsplit ? "Centering" : "Total")
            .arg(split_height).arg(m_itemVertSpacing).arg(row_height)
            .arg(split_height + m_itemVertSpacing + row_height)
            .arg(max_height));
        first_button = initial_first_button + button_shift;
        last_button  = initial_last_button + button_shift;
        first_item   = initial_first_item;
        last_item    = initial_last_item;
        return false;
    }

    if (*col_widths == nullptr)
    {
        /*
         * Allocate array to hold columns widths, now that we know
         * how many columns there are.
         */
        *col_widths = new int[static_cast<size_t>(col_cnt)];

        for (col_idx = 0; col_idx < col_cnt; ++col_idx)
            (*col_widths)[col_idx] = 0;

    }

    // Adjust for insertions on the front.
    first_button += button_shift;
    last_button += button_shift;

    // It fits, so so note max column widths
    MythUIStateType *realButton = nullptr;
    int buttonIdx = 0;

    if (grow_left)
    {
        begin = first_button;
        end = first_button + col_cnt;
    }
    else
    {
        end = last_button + 1;
        begin = end - col_cnt;
    }

    for (buttonIdx = begin, col_idx = 0;
         buttonIdx < end; ++buttonIdx, ++col_idx)
    {
        realButton = m_buttonList[buttonIdx];
        buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());
        if (!buttonstate)
            break;
        width = minButtonWidth(buttonstate->GetArea());

        (*col_widths)[col_idx] = std::max((*col_widths)[col_idx], width);

        // Make note of which column has the selected button
        if (selectedIdx == buttonIdx)
            selected_column = col_idx;
    }

    /*
      An underflow indicates we ran out of items, not that the
      buttons did not fit on the row.
    */
    if (total_height && underflow && col_cnt < m_columns)
        col_cnt = m_columns;

    return true;
}

/*
 * Dynamically layout columns
 */
bool MythUIButtonList::DistributeCols(int &first_button, int &last_button,
                                      int &first_item, int &last_item,
                                      int &selected_column, int &selected_row,
                                      int &skip_cols, int **col_widths,
                                      QList<int> & row_heights,
                                      int &top_height, int &bottom_height,
                                      bool &wrapped)
{
    int  col_cnt = 0;
    int  height = 0;
    int  end = 0;
    bool added = true;

    while (added)
    {
        added = false;

        if (wrapped)
            end = first_item;
        else
        {
            // Are we allowed to wrap when we run out of items?
            if (m_wrapStyle == WrapItems &&
                (m_scrollStyle == ScrollCenter ||
                 m_scrollStyle == ScrollGroupCenter) &&
                last_item + 1 == m_itemCount)
            {
                last_item = -1;
                wrapped = true;
                end = first_item;
            }
            else
            {
                end = m_itemCount;
            }
        }

        if (last_item + 1 < end)
        {
            // Does another row fit?
            if (DistributeRow(first_button, ++last_button,
                              first_item, ++last_item, selected_column,
                              skip_cols, false, true, col_widths, height,
                              top_height + bottom_height, bottom_height,
                              col_cnt, wrapped))
            {
                if (col_cnt < m_columns)
                    return false;  // Need to try again with fewer cols

                if (selected_row == -1 && selected_column != -1)
                    selected_row = row_heights.size();

                row_heights.push_back(height);
                bottom_height += (height + m_itemVertSpacing);
                added = true;
            }
            else
            {
                --last_button;
                --last_item;
            }
        }

        if (wrapped)
            end = last_item + 1;
        else
        {
            // Are we allowed to wrap when we run out of items?
            if (m_wrapStyle == WrapItems &&
                (m_scrollStyle == ScrollCenter ||
                 m_scrollStyle == ScrollGroupCenter) &&
                first_item == 0)
            {
                first_item = m_itemCount;
                wrapped = true;
                end = last_item + 1;
            }
            else
            {
                end = 0;
            }
        }

        if (first_item > end)
        {
            // Can we insert another row?
            if (DistributeRow(--first_button, last_button,
                              --first_item, last_item, selected_column,
                              skip_cols, true, false, col_widths, height,
                              top_height + bottom_height, top_height,
                              col_cnt, wrapped))
            {
                if (col_cnt < m_columns)
                    return false;  // Need to try again with fewer cols

                if (selected_row == -1 && selected_column != -1)
                    selected_row = row_heights.size();
                else if (selected_row != -1)
                    ++selected_row;

                row_heights.push_front(height);
                top_height += (height + m_itemVertSpacing);
                added = true;
            }
            else
            {
                ++first_button;
                ++first_item;
            }
        }
    }

    return true;
}

/*
 * Dynamically layout as many buttons as will fit in the area.
 */
bool MythUIButtonList::DistributeButtons(void)
{
    int  first_button = 0;
    int  last_button = 0;
    int  start_button = 0;
    int  start_item = m_selPosition;
    int  first_item = 0;
    int  last_item = 0;
    int  skip_cols = 0;
    int *col_widths = nullptr;
    int  col_cnt = 0;
    int  selected_column = -1;
    int  selected_row = -1;
    bool wrapped = false;
    bool grow_left = true;
    int height = 0;
    int top_height = 0;
    int bottom_height = 0;

    QList<int> row_heights;

    int alignment = IsShadowing() && m_shadowAlignment ?
                    *m_shadowAlignment : m_defaultAlignment;

    LOG(VB_GUI, LOG_DEBUG, QString("DistributeButtons: "
                                   "selected item %1 total items %2")
        .arg(start_item).arg(m_itemCount));

    // if there are no items to show make sure all the buttons are made invisible
    if (m_itemCount == 0)
    {
        for (int i = 0; i < m_buttonList.count(); ++i)
        {
            if (m_buttonList[i])
                m_buttonList[i]->SetVisible(false);
        }

        return false;
    }

    /*
     * Try fewer and fewer columns until each row can fit the same
     * number of columns.
     */
    for (m_columns = m_itemCount; m_columns > 0;)
    {
        first_item = last_item = start_item;

        /*
         * Drawing starts at start_button, and radiates from there.
         * Attempt to pick a start_button which will minimize the need for new
         * button allocations.
         */
        switch (m_scrollStyle)
        {
            case ScrollCenter:
            case ScrollGroupCenter:
                start_button = std::max((m_maxVisible / 2) - 1, 0);
                break;
            case ScrollFree:

                if (m_layout == LayoutGrid)
                {
                    start_button = 0;
                    first_item = last_item = 0;
                    grow_left = false;
                }
                else if (!m_buttonList.empty())
                {
                    if (m_itemCount - m_selPosition - 1 <
                        (m_buttonList.size() / 2))
                    {
                        start_button = m_buttonList.size() -
                                       (m_itemCount - m_selPosition) + 1;
                    }
                    else if (m_selPosition >
                             (m_buttonList.size() / 2))
                    {
                        start_button = (m_buttonList.size() / 2);
                    }
                    else
                    {
                        start_button = m_selPosition;
                    }
                }
                else
                {
                    start_button = 0;
                }

                break;
        }

        first_button = last_button = start_button;
        row_heights.clear();

        // Process row with selected button, and set starting val for m_columns.
        if (!DistributeRow(first_button, last_button,
                           first_item, last_item, selected_column,
                           skip_cols, grow_left, true, &col_widths,
                           height, 0, 0, col_cnt, wrapped))
        {
            delete[] col_widths;
            return false;
        }

        m_columns = col_cnt;

        if (m_layout == LayoutGrid && m_scrollStyle == ScrollFree)
        {
            /*
             * Now that we know how many columns there are, we can start
             * the grid layout for real.
             */
            start_item = (m_selPosition / m_columns) * m_columns;
            first_item = last_item = start_item;

            /*
             * Attempt to pick a start_button which will minimize the need
             * for new button allocations.
             */
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            start_button = std::max(m_buttonList.size() / 2, 0);
#else
            start_button = std::max(m_buttonList.size() / 2, static_cast<qsizetype>(0));
#endif
            start_button = (start_button / std::max(m_columns, 1)) * m_columns;

            if (start_button < m_itemCount / 2 &&
                m_itemCount - m_selPosition - 1 < m_buttonList.size() / 2)
                start_button += m_columns;

            first_button = last_button = start_button;

            // Now do initial row layout again with our new knowledge
            selected_column = selected_row = -1;

            if (!DistributeRow(first_button, last_button,
                               first_item, last_item, selected_column,
                               skip_cols, grow_left, true, &col_widths,
                               height, 0, 0, col_cnt, wrapped))
            {
                delete[] col_widths;
                return false;
            }
        }

        if (selected_column != -1)
            selected_row = 0;

        row_heights.push_back(height);

        if (m_scrollStyle == ScrollCenter)
            top_height = bottom_height = (height / 2);
        else
            bottom_height = height;

        if (m_layout == LayoutHorizontal)
            break;

        // As as many columns as will fit.
        if (DistributeCols(first_button, last_button,
                           first_item, last_item,
                           selected_column, selected_row,
                           skip_cols, &col_widths, row_heights,
                           top_height, bottom_height, wrapped))
            break; // Buttons fit on each row, so done

        delete[] col_widths;
        col_widths = nullptr;

        --m_columns;
        start_item = m_selPosition;
    }

    m_rows = row_heights.size();

    LOG(VB_GUI, LOG_DEBUG,
        QString("%1 rows, %2 columns fit inside parent area %3x%4")
        .arg(m_rows).arg(m_columns).arg(m_contentsRect.width())
        .arg(m_contentsRect.height()));

    if (col_widths == nullptr)
        return false;

    int total = 0;
    int left_spacing = 0;
    int right_spacing = 0;
    int top_spacing = 0;
    int bottom_spacing = 0;
    MythRect   min_rect;
    QString status_msg;

    /*
     * Calculate heights of buttons on each side of selected button
     */
    top_height = bottom_height = m_topRows = m_bottomRows = 0;

    status_msg = "Row heights: ";

    for (int row = 0; row < m_rows; ++row)
    {
        if (row != 0)
            status_msg += ", ";

        if (row == selected_row)
        {
            status_msg += '[';
            top_height += (row_heights[row] / 2);
            bottom_height += ((row_heights[row] / 2) + (row_heights[row] % 2));
        }
        else
        {
            if (bottom_height)
            {
                bottom_height += m_itemVertSpacing + row_heights[row];
                ++m_bottomRows;
            }
            else
            {
                top_height += row_heights[row] + m_itemVertSpacing;
                ++m_topRows;
            }
        }

        status_msg += QString("%1").arg(row_heights[row]);

        if (row == selected_row)
            status_msg += ']';
    }

    /*
     * How much extra space should there be between buttons?
     */
    if (m_arrange == ArrangeStack || m_layout == LayoutHorizontal)
    {
        // None
        top_spacing = bottom_spacing = 0;
    }
    else
    {
        if (m_rows < 2)
        {
            // Equal space on both sides of single row
            top_spacing = bottom_spacing =
                              (m_contentsRect.height() - top_height) / 2;
        }
        else
        {
            if (m_scrollStyle == ScrollCenter)
            {
                // Selected button needs to end up in the middle of area
                top_spacing = m_topRows ? (m_contentsRect.height() / 2 -
                                           top_height) / m_topRows : 0;
                bottom_spacing = m_bottomRows ? (m_contentsRect.height() / 2 -
                                                 bottom_height) / m_bottomRows : 0;

                if (m_arrange == ArrangeSpread)
                {
                    // Use same spacing on both sides of selected button
                    if (!m_topRows || top_spacing > bottom_spacing)
                        top_spacing = bottom_spacing;
                    else
                        bottom_spacing = top_spacing;
                }
            }
            else
            {
                // Buttons will be evenly spread out to fill entire area
                top_spacing = bottom_spacing = (m_contentsRect.height() -
                                                (top_height + bottom_height)) /
                                               (m_topRows + m_bottomRows);
            }
        }

        // Add in intra-button space size
        top_height += (top_spacing * m_topRows);
        bottom_height += (bottom_spacing * m_bottomRows);
    }

    /*
     * Calculate top margin
     */
    int y = m_contentsRect.y();

    if ((alignment & Qt::AlignVCenter) && m_arrange != ArrangeFill)
    {
        if (m_scrollStyle == ScrollCenter)
        {
            // Adjust to compensate for top height less than bottom height
            y += std::max(bottom_height - top_height, 0);
            total = std::max(top_height, bottom_height) * 2;
        }
        else
        {
            total = top_height + bottom_height;
        }

        // Adjust top margin so selected button ends up in the middle
        y += (std::max(m_contentsRect.height() - total, 2) / 2);
    }
    else if ((alignment & Qt::AlignBottom) && m_arrange == ArrangeStack)
    {
        // Adjust top margin so buttons are bottom justified
        y += std::max(m_contentsRect.height() -
                  (top_height + bottom_height), 0);
    }
    min_rect.setY(y);

    status_msg += QString(" spacing top %1 bottom %2 fixed %3 offset %4")
                  .arg(top_spacing).arg(bottom_spacing)
                  .arg(m_itemVertSpacing).arg(y);

    LOG(VB_GUI, LOG_DEBUG, status_msg);

    /*
     * Calculate width of buttons on each side of selected button
     */
    int left_width = 0;
    int right_width = 0;
    m_leftColumns = m_rightColumns = 0;

    status_msg = "Col widths: ";

    for (int col = 0; col < m_columns; ++col)
    {
        if (col != 0)
            status_msg += ", ";

        if (col == selected_column)
        {
            status_msg += '[';
            left_width += (col_widths[col] / 2);
            right_width += ((col_widths[col] / 2) + (col_widths[col] % 2));
        }
        else
        {
            if (right_width)
            {
                right_width += m_itemHorizSpacing + col_widths[col];
                ++m_rightColumns;
            }
            else
            {
                left_width  += col_widths[col] + m_itemHorizSpacing;
                ++m_leftColumns;
            }
        }

        status_msg += QString("%1").arg(col_widths[col]);

        if (col == selected_column)
            status_msg += ']';
    }

    /*
     * How much extra space should there be between buttons?
     */
    if (m_arrange == ArrangeStack || m_layout == LayoutVertical)
    {
        // None
        left_spacing = right_spacing = 0;
    }
    else
    {
        if (m_columns < 2)
        {
            // Equal space on both sides of single column
            left_spacing = right_spacing =
                               (m_contentsRect.width() - left_width) / 2;
        }
        else
        {
            if (m_scrollStyle == ScrollCenter)
            {
                // Selected button needs to end up in the middle
                left_spacing = m_leftColumns ? (m_contentsRect.width() / 2 -
                                                left_width) / m_leftColumns : 0;
                right_spacing = m_rightColumns ? (m_contentsRect.width() / 2 -
                                                  right_width) / m_rightColumns : 0;

                if (m_arrange == ArrangeSpread)
                {
                    // Use same spacing on both sides of selected button
                    if (!m_leftColumns || left_spacing > right_spacing)
                        left_spacing = right_spacing;
                    else
                        right_spacing = left_spacing;
                }
            }
            else
            {
                // Buttons will be evenly spread out to fill entire area
                left_spacing = right_spacing = (m_contentsRect.width() -
                                                (left_width + right_width)) /
                                               (m_leftColumns + m_rightColumns);
            }
        }

        // Add in intra-button space size
        left_width += (left_spacing * m_leftColumns);
        right_width += (right_spacing * m_rightColumns);
    }

    /*
     * Calculate left margin
     */
    int x_init = m_contentsRect.x();

    if ((alignment & Qt::AlignHCenter) && m_arrange != ArrangeFill)
    {
        if (m_scrollStyle == ScrollCenter)
        {
            // Compensate for left being smaller than right
            x_init += std::max(right_width - left_width, 0);
            total = std::max(left_width, right_width) * 2;
        }
        else
        {
            total = left_width + right_width;
        }

        // Adjust left margin so selected button ends up in the middle
        x_init += (std::max(m_contentsRect.width() - total, 2) / 2);
    }
    else if ((alignment & Qt::AlignRight) && m_arrange == ArrangeStack)
    {
        // Adjust left margin, so buttons are right justified
        x_init += std::max(m_contentsRect.width() -
                       (left_width + right_width), 0);
    }
    min_rect.setX(x_init);

    status_msg += QString(" spacing left %1 right %2 fixed %3 offset %4")
                  .arg(left_spacing).arg(right_spacing)
                  .arg(m_itemHorizSpacing).arg(x_init);
    LOG(VB_GUI, LOG_DEBUG, status_msg);

    top_spacing    += m_itemVertSpacing;
    bottom_spacing += m_itemVertSpacing;
    left_spacing   += m_itemHorizSpacing;
    right_spacing  += m_itemHorizSpacing;

    // Calculate position of each button
    int buttonIdx = first_button - skip_cols;
    int x = 0;
    int x_adj = 0;
    int y_adj = 0;

    int vertical_spacing = top_spacing;

    for (int row = 0; row < m_rows; ++row)
    {
        x = x_init;
        int horizontal_spacing = left_spacing;

        for (int col = 0; col < m_columns && buttonIdx <= last_button; ++col)
        {
            if (buttonIdx >= first_button)
            {
                MythUIStateType *realButton = m_buttonList[buttonIdx];
                auto *buttonstate = dynamic_cast<MythUIGroup *>
                    (realButton->GetCurrentState());
                if (!buttonstate)
                    break; // Not continue

                MythRect area = buttonstate->GetArea();

                // Center button within width of column
                if (alignment & Qt::AlignHCenter)
                    x_adj = (col_widths[col] - minButtonWidth(area)) / 2;
                else if (alignment & Qt::AlignRight)
                    x_adj = (col_widths[col] - minButtonWidth(area));
                else
                    x_adj = 0;
                if (m_layout == LayoutHorizontal)
                    x_adj -= area.x(); // Negate button's own offset

                // Center button within height of row.
                if (alignment & Qt::AlignVCenter)
                    y_adj = (row_heights[row] - minButtonHeight(area)) / 2;
                else if (alignment & Qt::AlignBottom)
                    y_adj = (row_heights[row] - minButtonHeight(area));
                else
                    y_adj = 0;
                if (m_layout == LayoutVertical)
                    y_adj -= area.y(); // Negate button's own offset

                // Set position of button
                realButton->SetPosition(x + x_adj, y + y_adj);
                realButton->SetVisible(true);

                if (col == selected_column)
                {
                    horizontal_spacing = right_spacing;
                    if (row == selected_row)
                        realButton->MoveToTop();
                }
            }
            x += col_widths[col] + horizontal_spacing;
            ++buttonIdx;
        }

        if (row == selected_row)
            vertical_spacing = bottom_spacing;

        y += row_heights[row] + vertical_spacing;
    }
    min_rect.setWidth(x - min_rect.x());
    min_rect.setHeight(y - min_rect.y());

    m_itemsVisible = m_columns * m_rows;

    // Hide buttons before first active button
    for (buttonIdx = 0; buttonIdx < first_button; ++buttonIdx)
        m_buttonList[buttonIdx]->SetVisible(false);

    // Hide buttons after last active buttons.
    for (buttonIdx = m_maxVisible - 1; buttonIdx > last_button; --buttonIdx)
        m_buttonList[buttonIdx]->SetVisible(false);

    // Set m_topPosition so arrows are displayed correctly.
    if (m_scrollStyle == ScrollCenter || m_scrollStyle == ScrollGroupCenter)
        m_topPosition = static_cast<int>(m_itemsVisible < m_itemCount);
    else
        m_topPosition = first_item;

    m_initiator = m_enableInitiator;
    if (m_minSize.isValid())
    {
        // Record the minimal area needed for the button list
        SetMinArea(min_rect);
    }

    delete[] col_widths;
    return true;
}

void MythUIButtonList::CalculateButtonPositions(void)
{
    if (m_buttonList.empty())
        return;

    int drawFromBottom = IsShadowing() && m_shadowDrawFromBottom ?
                         *m_shadowDrawFromBottom : m_defaultDrawFromBottom;

    int button = 0;

    switch (m_scrollStyle)
    {
        case ScrollCenter:
        case ScrollGroupCenter:
            m_topPosition = std::max(m_selPosition -
                                 (int)((float)m_itemsVisible / 2), 0);
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
                m_topPosition + m_itemsVisible <= m_selPosition)
            {
                if (m_layout == LayoutHorizontal)
                    m_topPosition = m_selPosition + adjust;
                else
                    m_topPosition = (m_selPosition + adjust) /
                                    m_columns * m_columns;
            }

            // Adjusted if last item is deleted
            if (((m_itemList.count() - m_topPosition) < m_itemsVisible) &&
                (m_selPosition - (m_itemsVisible - 1) < m_topPosition) &&
                 m_columns == 1)
                m_topPosition = m_selPosition - (m_itemsVisible - 1);

            m_topPosition = std::max(m_topPosition, 0);
            break;
        }
    }

    QList<MythUIButtonListItem *>::iterator it = m_itemList.begin() +
                                                 m_topPosition;

    if (m_scrollStyle == ScrollCenter || m_scrollStyle == ScrollGroupCenter)
    {
        if (m_selPosition <= m_itemsVisible / 2)
        {
            button = (m_itemsVisible / 2) - m_selPosition;

            if (m_wrapStyle == WrapItems && button > 0 &&
                m_itemCount >= m_itemsVisible)
            {
                it = m_itemList.end() - button;
                button = 0;
            }
        }
        else if ((m_itemCount - m_selPosition) < (m_itemsVisible / 2))
        {
            it = m_itemList.begin() + m_selPosition - (m_itemsVisible / 2);
        }
    }
    else if (drawFromBottom && m_itemCount < m_itemsVisible)
    {
        button = m_itemsVisible - m_itemCount;
    }

    for (int i = 0; i < button; ++i)
        m_buttonList[i]->SetVisible(false);

    bool seenSelected = false;

    MythUIStateType *realButton = nullptr;
    MythUIButtonListItem *buttonItem = nullptr;

    if (it < m_itemList.begin())
        it = m_itemList.begin();

    int curItem = it < m_itemList.end() ? GetItemPos(*it) : 0;

    while (it < m_itemList.end() && button < m_itemsVisible)
    {
        realButton = m_buttonList[button];
        buttonItem = *it;

        if (!realButton || !buttonItem)
            break;

        bool selected = false;

        if (!seenSelected && (curItem == m_selPosition))
        {
            seenSelected = true;
            selected = true;
        }

        m_buttonToItem[button] = buttonItem;
        buttonItem->SetToRealButton(realButton, selected);
        realButton->SetVisible(true);

        if (m_wrapStyle == WrapItems && it == (m_itemList.end() - 1) &&
            m_itemCount >= m_itemsVisible)
        {
            it = m_itemList.begin();
            curItem = 0;
        }
        else
        {
            ++it;
            ++curItem;
        }

        ++button;
    }

    for (; button < m_itemsVisible; ++button)
        m_buttonList[button]->SetVisible(false);
}

void MythUIButtonList::SanitizePosition(void)
{
    if (m_selPosition < 0)
        m_selPosition = (m_wrapStyle > WrapNone) ? m_itemList.size() - 1 : 0;
    else if (m_selPosition >= m_itemList.size())
        m_selPosition = (m_wrapStyle > WrapNone) ? 0 : m_itemList.size() - 1;
}

void MythUIButtonList::CalculateArrowStates()
{
    if (!m_initialized)
        Init();

    if (!m_initialized)
        return;

    if (m_clearing)
        return;

    m_needsUpdate = false;

    // mark the visible buttons as invisible
    QMap<int, MythUIButtonListItem*>::const_iterator i = m_buttonToItem.constBegin();
    while (i != m_buttonToItem.constEnd())
    {
        if (i.value())
            i.value()->setVisible(false);
        ++i;
    }

    // set topitem, top position
    SanitizePosition();
    m_buttonToItem.clear();

    if (m_arrange == ArrangeFixed)
        CalculateButtonPositions();
    else
        DistributeButtons();

    updateLCD();

    m_needsUpdate = false;

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

        if (m_topPosition + m_itemsVisible < m_itemCount)
            m_downArrow->DisplayState(MythUIStateType::Full);
        else
            m_downArrow->DisplayState(MythUIStateType::Off);

        m_upArrow->MoveToTop();
        m_downArrow->MoveToTop();
    }
}

void MythUIButtonList::ItemVisible(MythUIButtonListItem *item)
{
    if (item)
        emit itemVisible(item);
}

void MythUIButtonList::InsertItem(MythUIButtonListItem *item, int listPosition)
{
    bool wasEmpty = m_itemList.isEmpty();

    if (listPosition >= 0 && listPosition <= m_itemList.count())
    {
        m_itemList.insert(listPosition, item);

        if (listPosition <= m_selPosition)
            ++m_selPosition;

        if (listPosition <= m_topPosition)
            ++m_topPosition;
    }
    else
    {
        m_itemList.append(item);
    }

    ++m_itemCount;

    if (wasEmpty)
    {
        m_selPosition = m_topPosition = 0;
        emit itemSelected(item);
        emit DependChanged(false);
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

    QMap<int, MythUIButtonListItem*>::iterator it = m_buttonToItem.begin();
    while (it != m_buttonToItem.end())
    {
        if (it.value() == item)
        {
            m_buttonToItem.erase(it);
            break;
        }
        ++it;
    }

    if (curIndex < m_topPosition &&
        m_topPosition > 0)
    {
        // The removed item is before the visible part, move
        // everything up 1.  The visible part shouldn't appear to
        // change.
        --m_topPosition;
        --m_selPosition;
    }
    else if (curIndex < m_selPosition ||
             (m_selPosition == m_itemCount - 1 &&
              m_selPosition > 0))
    {
        // The removed item is visible and before the selected item or
        // the selected item is the last item but not the only item,
        // move the selected item up 1.
        --m_selPosition;
    }

    m_itemList.removeAt(curIndex);
    --m_itemCount;

    Update();

    if (m_selPosition < m_itemCount)
        emit itemSelected(m_itemList.at(m_selPosition));
    else
        emit itemSelected(nullptr);

    if (IsEmpty())
        emit DependChanged(true);
}

void MythUIButtonList::SetValueByData(const QVariant& data)
{
    if (!m_initialized)
        Init();

    for (auto *item : std::as_const(m_itemList))
    {
        if (item->GetData() == data)
        {
            SetItemCurrent(item);
            return;
        }
    }
}

void MythUIButtonList::SetItemCurrent(MythUIButtonListItem *item)
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

    if (!m_itemList.at(current)->isEnabled())
        return;

    if (current == m_selPosition &&
        (topPosition == -1 || topPosition == m_topPosition))
        return;

    m_topPosition = topPosition;

    if (topPosition > 0 && m_layout == LayoutGrid)
        m_topPosition -= (topPosition % m_columns);

    m_selPosition = current;

    Update();

    emit itemSelected(GetItemCurrent());
}

MythUIButtonListItem *MythUIButtonList::GetItemCurrent() const
{
    if (m_itemList.isEmpty() || m_selPosition >= m_itemList.size() ||
        m_selPosition < 0)
        return nullptr;

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

    return {};
}

QVariant MythUIButtonList::GetDataValue() const
{
    MythUIButtonListItem *item = GetItemCurrent();

    if (item)
        return item->GetData();

    return {};
}

MythRect MythUIButtonList::GetButtonArea(void) const
{
    if (m_contentsRect.isValid())
        return m_contentsRect;
    return m_area;
}

MythUIButtonListItem *MythUIButtonList::GetItemFirst() const
{
    if (!m_itemList.empty())
        return m_itemList[0];

    return nullptr;
}

MythUIButtonListItem *MythUIButtonList::GetItemNext(MythUIButtonListItem *item)
const
{
    QListIterator<MythUIButtonListItem *> it(m_itemList);

    if (!it.findNext(item))
        return nullptr;

    return it.previous();
}

int MythUIButtonList::GetCount() const
{
    return m_itemCount;
}

int MythUIButtonList::GetVisibleCount()
{
    if (m_needsUpdate)
    {
        CalculateArrowStates();
        SetScrollBarPosition();
    }

    return m_itemsVisible;
}

bool MythUIButtonList::IsEmpty() const
{
    return m_itemCount <= 0;
}

MythUIButtonListItem *MythUIButtonList::GetItemAt(int pos) const
{
    if (pos < 0 || pos >= m_itemList.size())
        return nullptr;

    return m_itemList.at(pos);
}

MythUIButtonListItem *MythUIButtonList::GetItemByData(const QVariant& data)
{
    if (!m_initialized)
        Init();

    for (auto *item : std::as_const(m_itemList))
    {
        if (item->GetData() == data)
            return item;
    }

    return nullptr;
}

int MythUIButtonList::GetItemPos(MythUIButtonListItem *item) const
{
    if (!item)
        return -1;

    return m_itemList.indexOf(item);
}

void MythUIButtonList::InitButton(int itemIdx, MythUIStateType* & realButton,
                                  MythUIButtonListItem* & buttonItem)
{
    buttonItem = m_itemList[itemIdx];

    if (m_maxVisible == 0)
    {
        QString name("buttonlist button 0");
        auto *button = new MythUIStateType(this, name);
        button->CopyFrom(m_buttontemplate);
        button->ConnectDependants(true);
        m_buttonList.append(button);
        ++m_maxVisible;
    }

    realButton = m_buttonList[0];
    m_buttonToItem[0] = buttonItem;
}

/*
 * PageUp and PageDown are helpers when Dynamic layout is being used.
 *
 * When buttons are layed out dynamically, the number of buttons on the next
 * page, may not equal the number of buttons on the current page.  Dynamic
 * layout is always center-weighted, so attempt to figure out which button
 * is near the middle on the next page.
 */

int MythUIButtonList::PageUp(void)
{
    int pos        = m_selPosition;
    int total      = 0;

    /*
     * /On the new page/
     * If the number of buttons before the selected button does not equal
     * the number of buttons after the selected button, this logic can
     * undershoot the new selected button.  That is better than overshooting
     * though.
     *
     * To fix this would require laying out the new page and then figuring
     * out which button should be selected, but this is already complex enough.
     */

    if (m_layout == LayoutHorizontal)
    {
        pos -= (m_leftColumns + 1);

        int max_width = m_contentsRect.width() / 2;

        for (; pos >= 0; --pos)
        {
            MythUIStateType *realButton = nullptr;
            MythUIButtonListItem *buttonItem = nullptr;
            InitButton(pos, realButton, buttonItem);
            buttonItem->SetToRealButton(realButton, true);
            auto *buttonstate = dynamic_cast<MythUIGroup *>
                          (realButton->GetCurrentState());

            if (buttonstate == nullptr)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "PageUp: Failed to query buttonlist state");
                return pos;
            }

            if (total + m_itemHorizSpacing +
                (buttonstate->GetArea().width() / 2) >= max_width)
                return pos + 1;

            buttonItem->SetToRealButton(realButton, false);
            buttonstate = dynamic_cast<MythUIGroup *>
                          (realButton->GetCurrentState());
            if (buttonstate)
                total += m_itemHorizSpacing + buttonstate->GetArea().width();
        }

        return 0;
    }

    // Grid or Vertical
    int dec = 1;

    if (m_layout == LayoutGrid)
    {
        /*
         * Adjusting using bottomRow:TopRow only works if new page
         * has the same ratio as the previous page, but that is common
         * with the grid layout, so go for it.  If themers start doing
         * grids where this is not true, then this will need to be modified.
         */
        pos -= (m_columns * (m_topRows + 2 +
                             std::max(m_bottomRows - m_topRows, 0)));
        dec = m_columns;
    }
    else
    {
        pos -= (m_topRows + 1);
        dec = 1;
    }

    int max_height = m_contentsRect.height() / 2;

    for (; pos >= 0; pos -= dec)
    {
        MythUIStateType *realButton = nullptr;
        MythUIButtonListItem *buttonItem = nullptr;
        InitButton(pos, realButton, buttonItem);
        buttonItem->SetToRealButton(realButton, true);
        auto *buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());

        if (buttonstate == nullptr)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "PageUp: Failed to query buttonlist state");
            return pos;
        }

        if (total + m_itemHorizSpacing +
            (buttonstate->GetArea().height() / 2) >= max_height)
            return pos + dec;

        buttonItem->SetToRealButton(realButton, false);
        buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());
        if (buttonstate)
            total += m_itemHorizSpacing + buttonstate->GetArea().height();
    }

    return 0;
}

int MythUIButtonList::PageDown(void)
{
    int pos        = m_selPosition;
    int num_items  = m_itemList.size();
    int total      = 0;

    /*
     * /On the new page/
     * If the number of buttons before the selected button does not equal
     * the number of buttons after the selected button, this logic can
     * undershoot the new selected button.  That is better than overshooting
     * though.
     *
     * To fix this would require laying out the new page and then figuring
     * out which button should be selected, but this is already complex enough.
     */

    if (m_layout == LayoutHorizontal)
    {
        pos += (m_rightColumns + 1);

        int max_width = m_contentsRect.width() / 2;

        for (; pos < num_items; ++pos)
        {
            MythUIStateType *realButton = nullptr;
            MythUIButtonListItem *buttonItem = nullptr;
            InitButton(pos, realButton, buttonItem);
            buttonItem->SetToRealButton(realButton, true);
            auto *buttonstate = dynamic_cast<MythUIGroup *>
                          (realButton->GetCurrentState());

            if (buttonstate == nullptr)
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "PageDown: Failed to query buttonlist state");
                return pos;
            }

            if (total + m_itemHorizSpacing +
                (buttonstate->GetArea().width() / 2) >= max_width)
                return pos - 1;

            buttonItem->SetToRealButton(realButton, false);
            buttonstate = dynamic_cast<MythUIGroup *>
                          (realButton->GetCurrentState());
            if (buttonstate)
                total += m_itemHorizSpacing + buttonstate->GetArea().width();
        }

        return num_items - 1;
    }

    // Grid or Vertical
    int inc = 1;

    if (m_layout == LayoutGrid)
    {
        /*
         * Adjusting using bottomRow:TopRow only works if new page
         * has the same ratio as the previous page, but that is common
         * with the grid layout, so go for it.  If themers start doing
         * grids where this is not true, then this will need to be modified.
         */
        pos += (m_columns * (m_bottomRows + 2 +
                             std::max(m_topRows - m_bottomRows, 0)));
        inc = m_columns;
    }
    else
    {
        pos += (m_bottomRows + 1);
        inc = 1;
    }

    int max_height = m_contentsRect.height() / 2;

    for (; pos < num_items; pos += inc)
    {
        MythUIStateType *realButton = nullptr;
        MythUIButtonListItem *buttonItem = nullptr;
        InitButton(pos, realButton, buttonItem);
        buttonItem->SetToRealButton(realButton, true);
        auto *buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());

        if (!buttonstate)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "PageDown: Failed to query buttonlist state");
            return pos;
        }

        if (total + m_itemHorizSpacing +
            (buttonstate->GetArea().height() / 2) >= max_height)
            return pos - inc;

        buttonItem->SetToRealButton(realButton, false);
        buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());
        if (buttonstate)
            total += m_itemHorizSpacing + buttonstate->GetArea().height();
    }

    return num_items - 1;
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

            FindEnabledUp(unit);

            break;

        case MoveColumn:
            if (pos % m_columns > 0)
            {
                --m_selPosition;
            }
            else if (m_wrapStyle == WrapFlowing)
            {
                if (m_selPosition == 0)
                    --m_selPosition = m_itemList.size() - 1;
                else
                    --m_selPosition;
            }
            else if (m_wrapStyle > WrapNone)
            {
                m_selPosition = pos + (m_columns - 1);
            }
            else if (m_wrapStyle == WrapCaptive)
            {
                return true;
            }

            FindEnabledUp(unit);

            break;

        case MoveRow:
            if (m_scrollStyle != ScrollFree)
            {
                m_selPosition -= m_columns;
                if (m_selPosition < 0)
                    m_selPosition += m_itemList.size();
                else
                    m_selPosition %= m_itemList.size();
            }
            else if ((pos - m_columns) >= 0)
            {
                m_selPosition -= m_columns;
            }
            else if (m_wrapStyle > WrapNone)
            {
                m_selPosition = (((m_itemList.size() - 1) / m_columns) *
                                m_columns) + pos;

                if ((m_selPosition / m_columns)
                    < ((m_itemList.size() - 1) / m_columns))
                    m_selPosition = m_itemList.size() - 1;

                if (m_layout == LayoutVertical)
                    m_topPosition = std::max(0, m_selPosition - m_itemsVisible + 1);
            }
            else if (m_wrapStyle == WrapCaptive)
            {
                return true;
            }

            FindEnabledUp(unit);

            break;

        case MovePage:
            if (m_arrange == ArrangeFixed)
                m_selPosition = std::max(0, m_selPosition - m_itemsVisible);
            else
                m_selPosition = PageUp();

            FindEnabledUp(unit);

            break;

        case MoveMid:
            m_selPosition = m_itemList.size() / 2;
            FindEnabledUp(unit);
            break;

        case MoveMax:
            m_selPosition = 0;
            FindEnabledUp(unit);
            break;

        case MoveByAmount:
            for (uint i = 0; i < amount; ++i)
            {
                if (m_selPosition > 0)
                    --m_selPosition;
                else if (m_wrapStyle > WrapNone)
                    m_selPosition = m_itemList.size() - 1;
            }

            FindEnabledUp(unit);

            break;
    }

    SanitizePosition();

    if (pos != m_selPosition)
    {
        Update();
        emit itemSelected(GetItemCurrent());
    }
    else
    {
        return false;
    }

    return true;
}


/**
 * If the current item is not enabled, find the next enabled one
 */
void MythUIButtonList::FindEnabledDown(MovementUnit unit)
{
    if (m_selPosition < 0 || m_selPosition >= m_itemList.size() ||
        m_itemList.at(m_selPosition)->isEnabled())
        return;

    int step = (unit == MoveRow) ? m_columns : 1;
    if (unit == MoveRow && m_wrapStyle == WrapFlowing)
        unit = MoveItem;
    if (unit == MoveColumn)
    {
        while (m_selPosition < m_itemList.size() &&
               (m_selPosition + 1) % m_columns > 0 &&
               !m_itemList.at(m_selPosition)->isEnabled())
            ++m_selPosition;

        if (m_itemList.at(m_selPosition)->isEnabled())
            return;

        if (m_wrapStyle > WrapNone)
        {
            m_selPosition = m_selPosition - (m_columns - 1);
            while ((m_selPosition + 1) % m_columns > 0 &&
                   !m_itemList.at(m_selPosition)->isEnabled())
                ++m_selPosition;
        }
    }
    else
    {
        while (!m_itemList.at(m_selPosition)->isEnabled() &&
               (m_selPosition < m_itemList.size() - step))
            m_selPosition += step;

        if (!m_itemList.at(m_selPosition)->isEnabled() &&
            m_wrapStyle > WrapNone)
        {
            m_selPosition = (m_selPosition + step) % m_itemList.size();

            while (!m_itemList.at(m_selPosition)->isEnabled() &&
                   (m_selPosition < m_itemList.size() - step))
                m_selPosition += step;
        }
    }
}

void MythUIButtonList::FindEnabledUp(MovementUnit unit)
{
    if (m_selPosition < 0 || m_selPosition >= m_itemList.size() ||
        m_itemList.at(m_selPosition)->isEnabled())
        return;

    int step = (unit == MoveRow) ? m_columns : 1;
    if (unit == MoveRow && m_wrapStyle == WrapFlowing)
        unit = MoveItem;
    if (unit == MoveColumn)
    {
        while (m_selPosition > 0 && (m_selPosition - 1) % m_columns > 0 &&
               !m_itemList.at(m_selPosition)->isEnabled())
            --m_selPosition;

        if (m_itemList.at(m_selPosition)->isEnabled())
            return;

        if (m_wrapStyle > WrapNone)
        {
            m_selPosition = m_selPosition + (m_columns - 1);
            while ((m_selPosition - 1) % m_columns > 0 &&
                   !m_itemList.at(m_selPosition)->isEnabled())
                --m_selPosition;
        }
    }
    else
    {
        while (!m_itemList.at(m_selPosition)->isEnabled() &&
               (m_selPosition - step >= 0))
            m_selPosition -= step;

        if (!m_itemList.at(m_selPosition)->isEnabled() &&
            m_wrapStyle > WrapNone)
        {
            m_selPosition = m_itemList.size() - 1;

            while (m_selPosition > 0 &&
                   !m_itemList.at(m_selPosition)->isEnabled() &&
                   (m_selPosition - step >= 0))
                m_selPosition -= step;
        }
    }
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

            FindEnabledDown(unit);

            break;

        case MoveColumn:
            if ((pos + 1) % m_columns > 0)
            {
                ++m_selPosition;
            }
            else if (m_wrapStyle == WrapFlowing)
            {
                if (m_selPosition < m_itemList.size() - 1)
                    ++m_selPosition;
                else
                    m_selPosition = 0;
            }
            else if (m_wrapStyle > WrapNone)
            {
                m_selPosition = pos - (m_columns - 1);
            }
            else if (m_wrapStyle == WrapCaptive)
            {
                return true;
            }

            FindEnabledDown(unit);

            break;

        case MoveRow:
            if (m_itemList.empty() || m_columns < 1)
                return true;
            if (m_scrollStyle != ScrollFree)
            {
                m_selPosition += m_columns;
                m_selPosition %= m_itemList.size();
            }
            else if (((m_itemList.size() - 1) / std::max(m_columns, 0))
                     > (pos / m_columns))
            {
                m_selPosition += m_columns;
                if (m_selPosition >= m_itemList.size())
                    m_selPosition = m_itemList.size() - 1;
            }
            else if (m_wrapStyle > WrapNone)
            {
                m_selPosition = (pos % m_columns);
            }
            else if (m_wrapStyle == WrapCaptive)
            {
                return true;
            }

            FindEnabledDown(unit);

            break;

        case MovePage:
            if (m_arrange == ArrangeFixed)
            {
                m_selPosition = std::min(m_itemCount - 1,
                                     m_selPosition + m_itemsVisible);
            }
            else
            {
                m_selPosition = PageDown();
            }

            FindEnabledDown(unit);

            break;

        case MoveMax:
            m_selPosition = m_itemCount - 1;
            FindEnabledDown(unit);
            break;

        case MoveByAmount:
            for (uint i = 0; i < amount; ++i)
            {
                if (m_selPosition < m_itemList.size() - 1)
                    ++m_selPosition;
                else if (m_wrapStyle > WrapNone)
                    m_selPosition = 0;
            }
            FindEnabledDown(unit);
            break;

        case MoveMid:
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
    {
        return false;
    }

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
    QList<MythUIButtonListItem *>::iterator it = m_itemList.begin();

    while (it != m_itemList.end())
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
    {
        insertat = m_selPosition + 1;
    }

    m_itemList.removeAt(oldpos);
    m_itemList.insert(insertat, item);

    if (up)
    {
        MoveUp();

        if (!dolast)
            MoveUp();
    }
    else
    {
        MoveDown();
    }

    return true;
}

void MythUIButtonList::SetAllChecked(MythUIButtonListItem::CheckState state)
{
    QMutableListIterator<MythUIButtonListItem *> it(m_itemList);

    while (it.hasNext())
        it.next()->setChecked(state);
}

void MythUIButtonList::Init()
{
    if (m_initialized)
        return;

    m_upArrow = dynamic_cast<MythUIStateType *>(GetChild("upscrollarrow"));
    m_downArrow = dynamic_cast<MythUIStateType *>(GetChild("downscrollarrow"));
    m_scrollBar = dynamic_cast<MythUIScrollBar *>(GetChild("scrollbar"));

    if (m_upArrow)
        m_upArrow->SetVisible(true);

    if (m_downArrow)
        m_downArrow->SetVisible(true);

    if (m_scrollBar)
        m_scrollBar->SetVisible(m_showScrollBar);

    m_contentsRect.CalculateArea(m_area);

    m_buttontemplate = dynamic_cast<MythUIStateType *>(GetChild("buttonitem"));

    if (!m_buttontemplate)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("(%1) Statetype buttonitem is "
                                         "required in mythuibuttonlist: %2")
            .arg(GetXMLLocation(), objectName()));
        return;
    }

    m_buttontemplate->SetVisible(false);

    MythRect buttonItemArea;

    MythUIGroup *buttonActiveState = dynamic_cast<MythUIGroup *>
                                     (m_buttontemplate->GetState("active"));

    if (buttonActiveState)
        buttonItemArea = buttonActiveState->GetArea();
    else
        buttonItemArea = m_buttontemplate->GetArea();

    buttonItemArea.CalculateArea(m_contentsRect);

    m_itemHeight = buttonItemArea.height();
    m_itemWidth = buttonItemArea.width();

    /*
     * If fixed spacing is defined, then use the "active" state size
     * to predictively determine the position of each button.
     */
    if (m_arrange == ArrangeFixed)
    {

        CalculateVisibleItems();

        int col = 1;
        int row = 1;

        for (int i = 0; i < m_itemsVisible; ++i)
        {
            QString name = QString("buttonlist button %1").arg(i);
            auto *button = new MythUIStateType(this, name);
            button->CopyFrom(m_buttontemplate);
            button->ConnectDependants(true);

            if (col > m_columns)
            {
                col = 1;
                ++row;
            }

            button->SetPosition(GetButtonPosition(col, row));
            ++col;

            m_buttonList.push_back(button);
        }
    }

    // The following is pretty much a hack for the benefit of MythGallery
    // it scales images based on the button size and we need to give it the
    // largest button state so that the images are not too small
    // This can be removed once the disk based image caching is added to
    // mythui, since the mythgallery thumbnail generator can be ditched.
    MythUIGroup *buttonSelectedState = dynamic_cast<MythUIGroup *>
                                       (m_buttontemplate->GetState("selected"));

    if (buttonSelectedState)
    {
        MythRect itemArea = buttonSelectedState->GetArea();
        itemArea.CalculateArea(m_contentsRect);

        m_itemHeight = std::max(m_itemHeight, itemArea.height());

        m_itemWidth = std::max(m_itemWidth, itemArea.width());
    }

    // End Hack

    m_initialized = true;
}

uint MythUIButtonList::ItemWidth(void)
{
    if (!m_initialized)
        Init();

    return static_cast<uint>(m_itemWidth);
}

uint MythUIButtonList::ItemHeight(void)
{
    if (!m_initialized)
        Init();

    return static_cast<uint>(m_itemHeight);
}

/**
 *  \copydoc MythUIType::keyPressEvent()
 */
bool MythUIButtonList::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = false;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    // Handle action remappings
    for (const QString& action : std::as_const(actions))
    {
        if (!m_actionRemap.contains(action))
            continue;

        QString key = m_actionRemap[action];
        if (key.isEmpty())
            return true;

        QKeySequence a(key);
        if (a.isEmpty())
            continue;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int keyCode = a[0];
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        QStringList parts = key.split('+');
        for (int j = 0; j < parts.count(); ++j)
        {
            if (parts[j].toUpper() == "CTRL")
                modifiers |= Qt::ControlModifier;
            if (parts[j].toUpper() == "SHIFT")
                modifiers |= Qt::ShiftModifier;
            if (parts[j].toUpper() == "ALT")
                modifiers |= Qt::AltModifier;
            if (parts[j].toUpper() == "META")
                modifiers |= Qt::MetaModifier;
        }
#else
        int keyCode = a[0].key();
        Qt::KeyboardModifiers modifiers = a[0].keyboardModifiers();
#endif

        QCoreApplication::postEvent(
            GetMythMainWindow(),
            new QKeyEvent(QEvent::KeyPress, keyCode, modifiers, key));
        QCoreApplication::postEvent(
            GetMythMainWindow(),
            new QKeyEvent(QEvent::KeyRelease, keyCode, modifiers, key));

        return true;
    }

    // handle actions for this container
    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        const QString& action = actions[i];
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
            {
                if (m_scrollStyle == ScrollFree)
                    handled = MoveDown(MoveColumn);
                else
                    handled = MoveDown(MoveItem);
            }
            else
            {
                handled = false;
            }
        }
        else if (action == "LEFT")
        {
            if (m_layout == LayoutHorizontal)
                handled = MoveUp(MoveItem);
            else if (m_layout == LayoutGrid)
            {
                if (m_scrollStyle == ScrollFree)
                    handled = MoveUp(MoveColumn);
                else
                    handled = MoveUp(MoveItem);
            }
            else
            {
                handled = false;
            }
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

            if (item && item->isEnabled())
                emit itemClicked(item);
        }
        else if (action == "SEARCH")
        {
            ShowSearchDialog();
        }
        else
        {
            handled = false;
        }
    }

    return handled;
}

/**
 *  \copydoc MythUIType::gestureEvent()
 */
bool MythUIButtonList::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;

    switch (event->GetGesture())
    {
        case MythGestureEvent::Click:
            {
                // We want the relative position of the click
                QPoint position = event->GetPosition() -
                                m_parent->GetArea().topLeft();

                MythUIType *type = GetChildAt(position, false, false);

                if (!type)
                    return false;

                auto *object = dynamic_cast<MythUIStateType *>(type);
                if (object)
                {
                    handled = true;
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
                        int pos = name.section(' ', 2, 2).toInt();
                        MythUIButtonListItem *item = m_buttonToItem.value(pos);

                        if (item)
                        {
                            if (item == GetItemCurrent())
                                emit itemClicked(item);
                            else
                                SetItemCurrent(item);
                        }
                    }
                    else
                    {
                        handled = false;
                    }
                }
            }
            break;

        case MythGestureEvent::Up:
        case MythGestureEvent::UpLeft:
        case MythGestureEvent::UpRight:
            if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
                handled = MoveUp(MoveRow);
            break;

        case MythGestureEvent::Down:
        case MythGestureEvent::DownLeft:
        case MythGestureEvent::DownRight:
            if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
                handled = MoveDown(MoveRow);
            break;

        case MythGestureEvent::Right:
            if (m_layout == LayoutHorizontal)
                handled = MoveDown(MoveItem);
            else if (m_layout == LayoutGrid)
            {
                if (m_scrollStyle == ScrollFree)
                    handled = MoveDown(MoveColumn);
                else
                    handled = MoveDown(MoveItem);
            }
            break;

        case MythGestureEvent::Left:
            if (m_layout == LayoutHorizontal)
                handled = MoveUp(MoveItem);
            else if (m_layout == LayoutGrid)
            {
                if (m_scrollStyle == ScrollFree)
                    handled = MoveUp(MoveColumn);
                else
                    handled = MoveUp(MoveItem);
            }
            break;

        default:
            break;
    }

    return handled;
}

class NextButtonListPageEvent : public QEvent
{
  public:
    NextButtonListPageEvent(int start, int pageSize) :
        QEvent(kEventType), m_start(start), m_pageSize(pageSize) {}
    const int m_start;
    const int m_pageSize;
    static const Type kEventType;
};

const QEvent::Type NextButtonListPageEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

void MythUIButtonList::customEvent(QEvent *event)
{
    if (event->type() == NextButtonListPageEvent::kEventType)
    {
        if (auto *npe = dynamic_cast<NextButtonListPageEvent*>(event); npe)
        {
            int cur = npe->m_start;
            for (; cur < npe->m_start + npe->m_pageSize && cur < GetCount(); ++cur)
            {
                const int loginterval = (cur < 1000 ? 100 : 500);
                if (cur > 200 && cur % loginterval == 0)
                    LOG(VB_GUI, LOG_INFO,
                        QString("Build background buttonlist item %1").arg(cur));
                emit itemLoaded(GetItemAt(cur));
            }
            m_nextItemLoaded = cur;
            if (cur < GetCount())
                LoadInBackground(cur, npe->m_pageSize);
        }
    }
}

void MythUIButtonList::LoadInBackground(int start, int pageSize)
{
    m_nextItemLoaded = start;
    QCoreApplication::
        postEvent(this, new NextButtonListPageEvent(start, pageSize));
}

int MythUIButtonList::StopLoad(void)
{
    QCoreApplication::
        removePostedEvents(this, NextButtonListPageEvent::kEventType);
    return m_nextItemLoaded;
}

QPoint MythUIButtonList::GetButtonPosition(int column, int row) const
{
    int x = m_contentsRect.x() +
            ((column - 1) * (m_itemWidth + m_itemHorizSpacing));
    int y = m_contentsRect.y() +
            ((row - 1) * (m_itemHeight + m_itemVertSpacing));

    return {x, y};
}

void MythUIButtonList::CalculateVisibleItems(void)
{
    m_itemsVisible = 0;
    m_rows = 0;
    m_columns = 0;

    if ((m_layout == LayoutHorizontal) || (m_layout == LayoutGrid))
    {
        int x = 0;

        while (x <= m_contentsRect.width() - m_itemWidth)
        {
            x += m_itemWidth + m_itemHorizSpacing;
            ++m_columns;
        }
    }

    if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
    {
        int y = 0;

        while (y <= m_contentsRect.height() - m_itemHeight)
        {
            y += m_itemHeight + m_itemVertSpacing;
            ++m_rows;
        }
    }

    if (m_rows <= 0)
        m_rows = 1;

    if (m_columns <= 0)
        m_columns = 1;

    m_itemsVisible = m_columns * m_rows;
}

void MythUIButtonList::SetButtonArea(const MythRect &rect)
{
    if (rect == m_contentsRect)
        return;

    m_contentsRect = rect;

    if (m_area.isValid())
        m_contentsRect.CalculateArea(m_area);
    else if (m_parent)
        m_contentsRect.CalculateArea(m_parent->GetFullArea());
    else
        m_contentsRect.CalculateArea(GetMythMainWindow()->GetUIScreenRect());
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIButtonList::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "buttonarea")
        SetButtonArea(parseRect(element));
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
    else if (element.tagName() == "arrange")
    {
        QString arrange = getFirstText(element).toLower();

        if (arrange == "fill")
            m_arrange = ArrangeFill;
        else if (arrange == "spread")
            m_arrange = ArrangeSpread;
        else if (arrange == "stack")
            m_arrange = ArrangeStack;
        else
            m_arrange = ArrangeFixed;

    }
    else if (element.tagName() == "align")
    {
        QString align = getFirstText(element).toLower();
        m_defaultAlignment = parseAlignment(align);
    }
    else if (element.tagName() == "shadowalign")
    {
        QString align = getFirstText(element).toLower();
        m_shadowAlignment = parseAlignment(align);
    }
    else if (element.tagName() == "scrollstyle")
    {
        QString layout = getFirstText(element).toLower();

        if (layout == "center")
            m_scrollStyle = ScrollCenter;
        else if (layout == "groupcenter")
            m_scrollStyle = ScrollGroupCenter;
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
        else if (wrapstyle == "flowing")
            m_wrapStyle = WrapFlowing;
        else if (wrapstyle == "items")
            m_wrapStyle = WrapItems;
    }
    else if (element.tagName() == "showarrow")
    {
        m_showArrow = parseBool(element);
    }
    else if (element.tagName() == "showscrollbar")
    {
        m_showScrollBar = parseBool(element);
    }
    else if (element.tagName() == "spacing")
    {
        m_itemHorizSpacing = NormX(getFirstText(element).toInt());
        m_itemVertSpacing = NormY(getFirstText(element).toInt());
    }
    else if (element.tagName() == "drawfrombottom")
    {
        m_defaultDrawFromBottom = parseBool(element);

        if (m_defaultDrawFromBottom)
            m_defaultAlignment |= Qt::AlignBottom;
    }
    else if (element.tagName() == "shadowdrawfrombottom")
    {
        m_shadowDrawFromBottom = parseBool(element);

        if (*m_shadowDrawFromBottom)
            m_shadowAlignment = *m_shadowAlignment | Qt::AlignBottom;
    }
    else if (element.tagName() == "searchposition")
    {
        m_searchPosition = parsePoint(element);
    }
    else if (element.tagName() == "triggerevent")
    {
        QString trigger = getFirstText(element);
        if (!trigger.isEmpty())
        {
            QString action = element.attribute("action", "");
            if (action.isEmpty())
            {
                m_actionRemap[trigger] = "";
            }
            else
            {
                QString context = element.attribute("context", "");
                QString keylist = MythMainWindow::GetKey(context, action);
                QStringList keys = keylist.split(',', Qt::SkipEmptyParts);
                if (!keys.empty())
                    m_actionRemap[trigger] = keys[0];
            }
        }
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/**
 *  \copydoc MythUIType::DrawSelf()
 */
void MythUIButtonList::DrawSelf(MythPainter * /*p*/, int /*xoffset*/, int /*yoffset*/,
                                int /*alphaMod*/, QRect /*clipRect*/)
{
    if (m_needsUpdate)
    {
        CalculateArrowStates();
        SetScrollBarPosition();
    }
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIButtonList::CreateCopy(MythUIType *parent)
{
    auto *lb = new MythUIButtonList(parent, objectName());
    lb->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIButtonList::CopyFrom(MythUIType *base)
{
    auto *lb = dynamic_cast<MythUIButtonList *>(base);
    if (!lb)
        return;

    m_layout = lb->m_layout;
    m_arrange = lb->m_arrange;
    m_defaultAlignment = lb->m_defaultAlignment;
    m_shadowAlignment = lb->m_shadowAlignment;

    m_contentsRect = lb->m_contentsRect;

    m_itemHeight = lb->m_itemHeight;
    m_itemWidth = lb->m_itemWidth;
    m_itemHorizSpacing = lb->m_itemHorizSpacing;
    m_itemVertSpacing = lb->m_itemVertSpacing;
    m_itemsVisible = lb->m_itemsVisible;
    m_maxVisible = lb->m_maxVisible;

    m_active = lb->m_active;
    m_showArrow = lb->m_showArrow;
    m_showScrollBar = lb->m_showScrollBar;

    m_defaultDrawFromBottom = lb->m_defaultDrawFromBottom;
    m_shadowDrawFromBottom = lb->m_shadowDrawFromBottom;

    m_scrollStyle = lb->m_scrollStyle;
    m_wrapStyle = lb->m_wrapStyle;

    m_clearing = false;
    m_selPosition = m_topPosition = m_itemCount = 0;

    m_searchPosition = lb->m_searchPosition;
    m_searchFields = lb->m_searchFields;

    MythUIType::CopyFrom(base);

    m_upArrow = dynamic_cast<MythUIStateType *>(GetChild("upscrollarrow"));
    m_downArrow = dynamic_cast<MythUIStateType *>(GetChild("downscrollarrow"));
    m_scrollBar = dynamic_cast<MythUIScrollBar *>(GetChild("scrollbar"));

    for (int i = 0; i < m_itemsVisible; ++i)
    {
        QString name = QString("buttonlist button %1").arg(i);
        DeleteChild(name);
    }

    m_buttonList.clear();

    m_actionRemap = lb->m_actionRemap;

    m_initialized = false;
}

/**
 *  \copydoc MythUIType::Finalize()
 */
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
    if (!m_hasFocus)
        return;

    LCD *lcddev = LCD::Get();

    if (lcddev == nullptr)
        return;

    // Build a list of the menu items
    QList<LCDMenuItem> menuItems;

    auto start = std::max(0, m_selPosition - lcddev->getLCDHeight());
    auto end = std::min(m_itemCount, start + (lcddev->getLCDHeight() * 2));

    for (int r = start; r < end; ++r)
    {
        bool selected = r == GetCurrentPos();

        MythUIButtonListItem *item = GetItemAt(r);
        CHECKED_STATE state = NOTCHECKABLE;

        if (item->checkable())
            state = (item->state() == MythUIButtonListItem::NotChecked) ? UNCHECKED : CHECKED;

        QString text;

        for (int x = 0; x < m_lcdColumns.count(); ++x)
        {
            if (!m_lcdColumns[x].isEmpty() && item->m_strings.contains(m_lcdColumns[x]))
            {
                // named text column
                TextProperties props = item->m_strings[m_lcdColumns[x]];

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

void MythUIButtonList::ShowSearchDialog(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    auto *dlg = new SearchButtonListDialog(popupStack, "MythSearchListDialog", this, "");

    if (dlg->Create())
    {
        if (m_searchPosition.x() != -2 || m_searchPosition.y() != -2)
        {
            int x = m_searchPosition.x();
            int y = m_searchPosition.y();
            QRect screenArea = GetMythMainWindow()->GetUIScreenRect();
            QRect dialogArea = dlg->GetArea();

            if (x == -1)
                x = (screenArea.width() - dialogArea.width()) / 2;

            if (y == -1)
                y = (screenArea.height() - dialogArea.height()) / 2;

            dlg->SetPosition(x, y);
        }

        popupStack->AddScreen(dlg);
    }
    else
    {
        delete dlg;
    }
}

bool MythUIButtonList::Find(const QString &searchStr, bool startsWith)
{
    m_searchStr = searchStr;
    m_searchStartsWith = startsWith;
    return DoFind(false, true);
}

bool MythUIButtonList::FindNext(void)
{
    return DoFind(true, true);
}

bool MythUIButtonList::FindPrev(void)
{
    return DoFind(true, false);
}

bool MythUIButtonList::DoFind(bool doMove, bool searchForward)
{
    if (m_searchStr.isEmpty())
        return true;

    if (GetCount() == 0)
        return false;

    int startPos = GetCurrentPos();
    int currPos = startPos;
    bool found = false;

    if (doMove)
    {
        if (searchForward)
        {
            ++currPos;

            if (currPos >= GetCount())
                currPos = 0;
        }
        else
        {
            --currPos;

            if (currPos < 0)
                currPos = GetCount() - 1;
        }
    }

    while (true)
    {
        found = GetItemAt(currPos)->FindText(m_searchStr, m_searchFields, m_searchStartsWith);

        if (found)
        {
            SetItemCurrent(currPos);
            return true;
        }

        if (searchForward)
        {
            ++currPos;

            if (currPos >= GetCount())
                currPos = 0;
        }
        else
        {
            --currPos;

            if (currPos < 0)
                currPos = GetCount() - 1;
        }

        if (startPos == currPos)
            break;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////

MythUIButtonListItem::MythUIButtonListItem(MythUIButtonList *lbtype,
                                           QString text, QString image,
                                           bool checkable, CheckState state,
                                           bool showArrow, int listPosition)
    : m_parent(lbtype), m_text(std::move(text)), m_imageFilename(std::move(image)),
      m_checkable(checkable), m_state(state), m_showArrow(showArrow)
{
    if (!lbtype)
        LOG(VB_GENERAL, LOG_ERR, "Cannot add a button to a non-existent list!");

    if (state >= NotChecked)
        m_checkable = true;

    if (m_parent)
        m_parent->InsertItem(this, listPosition);
}

MythUIButtonListItem::MythUIButtonListItem(MythUIButtonList *lbtype,
                                           const QString &text,
                                           QVariant data, int listPosition)
{
    if (!lbtype)
        LOG(VB_GENERAL, LOG_ERR, "Cannot add a button to a non-existent list!");

    m_parent    = lbtype;
    m_text      = text;
    m_data      = std::move(data);

    m_image     = nullptr;

    m_checkable = false;
    m_state     = CantCheck;
    m_showArrow = false;
    m_isVisible = false;
    m_enabled   = true;

    if (m_parent)
        m_parent->InsertItem(this, listPosition);
}

MythUIButtonListItem::~MythUIButtonListItem()
{
    if (m_parent)
        m_parent->RemoveItem(this);

    if (m_image)
        m_image->DecrRef();

    QMap<QString, MythImage*>::iterator it;
    for (it = m_images.begin(); it != m_images.end(); ++it)
    {
        if (*it)
            (*it)->DecrRef();
    }
    m_images.clear();
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
    {
        m_text = text;
    }

    if (m_parent && m_isVisible)
        m_parent->Update();
}

void MythUIButtonListItem::SetTextFromMap(const InfoMap &infoMap,
                                          const QString &state)
{
    InfoMap::const_iterator map_it = infoMap.begin();

    while (map_it != infoMap.end())
    {
        TextProperties textprop;
        textprop.text = (*map_it);
        textprop.state = state;
        m_strings[map_it.key()] = textprop;
        ++map_it;
    }

    if (m_parent && m_isVisible)
        m_parent->Update();
}

void MythUIButtonListItem::SetTextFromMap(const QMap<QString, TextProperties> &stringMap)
{
    m_strings.clear();
    m_strings = stringMap;
}

void MythUIButtonListItem::SetTextCb(muibCbFn fn, void *data)
{
    m_textCb.fn = fn;
    m_textCb.data = data;
}

QString MythUIButtonListItem::GetText(const QString &name) const
{
    if (name.isEmpty())
        return m_text;
    if (m_textCb.fn != nullptr)
    {
        QString result = m_textCb.fn(name, m_textCb.data);
        if (!result.isEmpty())
            return result;
    }
    if (m_strings.contains(name))
        return m_strings[name].text;
    return {};
}

TextProperties MythUIButtonListItem::GetTextProp(const QString &name) const
{
    if (name.isEmpty())
        return {m_text, ""};
    if (m_textCb.fn != nullptr)
    {
        QString result = m_textCb.fn(name, m_textCb.data);
        if (!result.isEmpty())
            return {result, ""};
    }
    if (m_strings.contains(name))
        return m_strings[name];
    return {};
}

bool MythUIButtonListItem::FindText(const QString &searchStr, const QString &fieldList,
                                    bool startsWith) const
{
    if (fieldList.isEmpty())
    {
        if (startsWith)
            return m_text.startsWith(searchStr, Qt::CaseInsensitive);
        return m_text.contains(searchStr, Qt::CaseInsensitive);
    }
    if (fieldList == "**ALL**")
    {
        if (startsWith)
        {
            if (m_text.startsWith(searchStr, Qt::CaseInsensitive))
                return true;
        }
        else
        {
            if (m_text.contains(searchStr, Qt::CaseInsensitive))
                return true;
        }

        QMap<QString, TextProperties>::const_iterator i = m_strings.constBegin();

        while (i != m_strings.constEnd())
        {
            if (startsWith)
            {
                if (i.value().text.startsWith(searchStr, Qt::CaseInsensitive))
                    return true;
            }
            else
            {
                if (i.value().text.contains(searchStr, Qt::CaseInsensitive))
                    return true;
            }

            ++i;
        }
    }
    else
    {
        QStringList fields = fieldList.split(',', Qt::SkipEmptyParts);
        for (int x = 0; x < fields.count(); ++x)
        {
            if (m_strings.contains(fields.at(x).trimmed()))
            {
                if (startsWith)
                {
                    if (m_strings[fields.at(x)].text.startsWith(searchStr, Qt::CaseInsensitive))
                        return true;
                }
                else
                {
                    if (m_strings[fields.at(x)].text.contains(searchStr, Qt::CaseInsensitive))
                        return true;
                }
            }
        }
    }

    return false;
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
    {
        m_fontState = state;
    }

    if (m_parent && m_isVisible)
        m_parent->Update();
}

void MythUIButtonListItem::SetImage(MythImage *image, const QString &name)
{
    if (image)
        image->IncrRef();

    if (!name.isEmpty())
    {
        QMap<QString, MythImage*>::iterator it = m_images.find(name);
        if (it != m_images.end())
        {
            (*it)->DecrRef();
            if (image)
                *it = image;
            else
                m_images.erase(it);
        }
        else if (image)
        {
            m_images[name] = image;
        }
    }
    else
    {
        if (m_image)
            m_image->DecrRef();
        m_image = image;
    }

    if (m_parent && m_isVisible)
        m_parent->Update();
}

void MythUIButtonListItem::SetImageFromMap(const InfoMap &imageMap)
{
    m_imageFilenames.clear();
    m_imageFilenames = imageMap;
}

void MythUIButtonListItem::SetImageCb(muibCbFn fn, void *data)
{
    m_imageCb.fn = fn;
    m_imageCb.data = data;
}

MythImage *MythUIButtonListItem::GetImage(const QString &name)
{
    if (!name.isEmpty())
    {
        QMap<QString, MythImage*>::iterator it = m_images.find(name);
        if (it != m_images.end())
        {
            (*it)->IncrRef();
            return (*it);
        }
    }
    else if (m_image)
    {
        m_image->IncrRef();
        return m_image;
    }

    return nullptr;
}

void MythUIButtonListItem::SetImage(
    const QString &filename, const QString &name, bool force_reload)
{
    bool do_update = force_reload;

    if (!name.isEmpty())
    {
        InfoMap::iterator it = m_imageFilenames.find(name);

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

    if (m_parent && do_update && m_isVisible)
        m_parent->Update();
}

QString MythUIButtonListItem::GetImageFilename(const QString &name) const
{
    if (name.isEmpty())
        return m_imageFilename;

    if (m_imageCb.fn != nullptr)
    {
        QString result = m_imageCb.fn(name, m_imageCb.data);
        if (!result.isEmpty())
            return result;
    }

    InfoMap::const_iterator it = m_imageFilenames.find(name);

    if (it != m_imageFilenames.end())
        return *it;

    return {};
}

void MythUIButtonListItem::SetProgress1(int start, int total, int used)
{
    m_progress1.used  = used;
    m_progress1.start = start;
    m_progress1.total = total;

    if (m_parent && m_isVisible)
        m_parent->Update();
}

void MythUIButtonListItem::SetProgress2(int start, int total, int used)
{
    m_progress2.used  = used;
    m_progress2.start = start;
    m_progress2.total = total;

    if (m_parent && m_isVisible)
        m_parent->Update();
}

void MythUIButtonListItem::DisplayState(const QString &state,
                                        const QString &name)
{
    if (name.isEmpty())
        return;

    bool do_update = false;
    InfoMap::iterator it = m_states.find(name);

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

    if (m_parent && do_update && m_isVisible)
        m_parent->Update();
}

void MythUIButtonListItem::SetStatesFromMap(const InfoMap &stateMap)
{
    m_states.clear();
    m_states = stateMap;
}

void MythUIButtonListItem::SetStateCb(muibCbFn fn, void *data)
{
    m_stateCb.fn = fn;
    m_stateCb.data = data;
}

QString MythUIButtonListItem::GetState(const QString &name)
{
    if (name.isEmpty())
        return {};
    if (m_stateCb.fn != nullptr)
    {
        QString result = m_stateCb.fn(name, m_textCb.data);
        if (!result.isEmpty())
            return result;
    }
    if (m_states.contains(name))
        return m_states[name];
    return {};
}

bool MythUIButtonListItem::checkable() const
{
    return m_checkable;
}

MythUIButtonListItem::CheckState MythUIButtonListItem::state() const
{
    return m_state;
}

MythUIButtonList *MythUIButtonListItem::parent() const
{
    return m_parent;
}

void MythUIButtonListItem::setChecked(MythUIButtonListItem::CheckState state)
{
    if (!m_checkable || m_state == state)
        return;

    m_state = state;

    if (m_parent && m_isVisible)
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

bool MythUIButtonListItem::isEnabled() const
{
    return m_enabled;
}

void MythUIButtonListItem::setEnabled(bool flag)
{
    m_enabled = flag;
}

void MythUIButtonListItem::SetData(QVariant data)
{
    m_data = std::move(data);
}

QVariant MythUIButtonListItem::GetData()
{
    return m_data;
}

bool MythUIButtonListItem::MoveUpDown(bool flag)
{
    if (m_parent)
        return m_parent->MoveItemUpDown(this, flag);
    return false;
}

void MythUIButtonListItem::DoButtonText (MythUIText *buttontext)
{
    if (!buttontext)
        return;

    buttontext->SetText(m_text);
    buttontext->SetFontState(m_fontState);
}

void MythUIButtonListItem::DoButtonImage (MythUIImage *buttonimage)
{
    if (!buttonimage)
        return;

    if (!m_imageFilename.isEmpty())
    {
        buttonimage->SetFilename(m_imageFilename);
        buttonimage->Load();
    }
    else if (m_image)
    {
        buttonimage->SetImage(m_image);
    }
}

void MythUIButtonListItem::DoButtonArrow (MythUIImage *buttonarrow) const
{
    if (!buttonarrow)
        return;
    buttonarrow->SetVisible(m_showArrow);
}

void MythUIButtonListItem::DoButtonCheck (MythUIStateType *buttoncheck)
{
    if (!buttoncheck)
        return;

    buttoncheck->SetVisible(m_checkable);

    if (!m_checkable)
        return;

    if (m_state == NotChecked)
        buttoncheck->DisplayState(MythUIStateType::Off);
    else if (m_state == HalfChecked)
        buttoncheck->DisplayState(MythUIStateType::Half);
    else
        buttoncheck->DisplayState(MythUIStateType::Full);
}

void MythUIButtonListItem::DoButtonProgress1 (MythUIProgressBar *buttonprogress) const
{
    if (!buttonprogress)
        return;

    buttonprogress->Set(m_progress1.start, m_progress1.total, m_progress1.used);
}

void MythUIButtonListItem::DoButtonProgress2 (MythUIProgressBar *buttonprogress) const
{
    if (!buttonprogress)
        return;

    buttonprogress->Set(m_progress2.start, m_progress2.total, m_progress2.used);
}

void MythUIButtonListItem::DoButtonLookupText (MythUIText *text,
                                               const TextProperties& textprop)
{
    if (!text)
        return;

    QString newText = text->GetTemplateText();

    static const QRegularExpression re {R"(%(([^\|%]+)?\||\|(.))?([\w#]+)(\|(.+?))?%)",
        QRegularExpression::DotMatchesEverythingOption};

    if (!newText.isEmpty() && newText.contains(re))
    {
        QString tempString = newText;

        QRegularExpressionMatchIterator i = re.globalMatch(newText);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            QString key = match.captured(4).toLower().trimmed();
            QString replacement;
            QString value = GetText(key);

            if (!value.isEmpty())
            {
                replacement = QString("%1%2%3%4")
                    .arg(match.captured(2),
                         match.captured(3),
                         value,
                         match.captured(6));
            }

            tempString.replace(match.captured(0), replacement);
        }

        newText = tempString;
    }
    else
    {
        newText = textprop.text;
    }

    if (newText.isEmpty())
        text->Reset();
    else
        text->SetText(newText);

    text->SetFontState(textprop.state.isEmpty() ? m_fontState : textprop.state);
}

void MythUIButtonListItem::DoButtonLookupFilename (MythUIImage *image, const QString& filename)
{
    if (!image)
        return;

    if (!filename.isEmpty())
    {
        image->SetFilename(filename);
        image->Load();
    }
    else
    {
        image->Reset();
    }
}

void MythUIButtonListItem::DoButtonLookupImage (MythUIImage *uiimage, MythImage *image)
{
    if (!uiimage)
        return;

    if (image)
        uiimage->SetImage(image);
    else
        uiimage->Reset();
}

void MythUIButtonListItem::DoButtonLookupState (MythUIStateType *statetype, const QString& name)
{
    if (!statetype)
        return;

    if (!statetype->DisplayState(name))
        statetype->Reset();
}

void MythUIButtonListItem::SetToRealButton(MythUIStateType *button,
                                           bool selected)
{
    if (!m_parent)
        return;

    m_parent->ItemVisible(this);
    m_isVisible = true;

    QString state;

    if (!m_parent->IsEnabled())
        state = "disabled";
    else if (!m_enabled)
    {
        state = m_parent->m_active ? "disabledactive" : "disabledinactive";
    }
    else if (selected)
    {
        button->MoveToTop();
        state = m_parent->m_active ? "selectedactive" : "selectedinactive";
    }
    else
    {
        state = m_parent->m_active ? "active" : "inactive";
    }

    if (m_parent->IsShadowing())
    {
        if (state == "inactive" && button->GetState("shadow"))
            state = "shadow";
        else if (state == "selectedinactive" &&
                 button->GetState("selectedshadow"))
            state = "selectedshadow";
    }

    // Begin compatibility code
    // Attempt to fallback if the theme is missing certain states
    if (state == "disabled" && !button->GetState(state))
    {
        LOG(VB_GUI, LOG_WARNING, "Theme Error: Missing buttonlist state: disabled");
        state = "inactive";
    }

    if (state == "inactive" && !button->GetState(state))
    {
        LOG(VB_GUI, LOG_WARNING, "Theme Error: Missing buttonlist state: inactive");
        state = "active";
    }
    // End compatibility code

    auto *buttonstate = dynamic_cast<MythUIGroup *>(button->GetState(state));
    if (!buttonstate)
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("Theme Error: Missing buttonlist state: %1")
            .arg(state));
        return;
    }

    buttonstate->Reset();

    QList<MythUIType *> descendants = buttonstate->GetAllDescendants();
    for (MythUIType *obj : std::as_const(descendants))
    {
        QString name = obj->objectName();
        if (name == "buttontext")
            DoButtonText(dynamic_cast<MythUIText *>(obj));
        else if (name == "buttonimage")
            DoButtonImage(dynamic_cast<MythUIImage *>(obj));
        else if (name == "buttonarrow")
            DoButtonArrow(dynamic_cast<MythUIImage *>(obj));
        else if (name == "buttoncheck")
            DoButtonCheck(dynamic_cast<MythUIStateType *>(obj));
        else if (name == "buttonprogress1")
            DoButtonProgress1(dynamic_cast<MythUIProgressBar *>(obj));
        else if (name == "buttonprogress2")
            DoButtonProgress2(dynamic_cast<MythUIProgressBar *>(obj));

        TextProperties textprop = GetTextProp(name);
        if (!textprop.text.isEmpty())
            DoButtonLookupText(dynamic_cast<MythUIText *>(obj), textprop);

        QString filename = GetImageFilename(name);
        if (!filename.isEmpty())
            DoButtonLookupFilename (dynamic_cast<MythUIImage *>(obj), filename);

        if (m_images.contains(name))
            DoButtonLookupImage(dynamic_cast<MythUIImage *>(obj), m_images[name]);

        QString luState = GetState(name);
        if (!luState.isEmpty())
            DoButtonLookupState(dynamic_cast<MythUIStateType *>(obj), luState);
    }

    // There is no need to check the return value here, since we already
    // checked that the state exists with GetState() earlier
    button->DisplayState(state);
}

//---------------------------------------------------------
// SearchButtonListDialog
//---------------------------------------------------------
bool SearchButtonListDialog::Create(void)
{
    if (!CopyWindowFromBase("MythSearchListDialog", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_searchEdit,  "searchedit", &err);
    UIUtilE::Assign(this, m_prevButton,  "prevbutton", &err);
    UIUtilE::Assign(this, m_nextButton,  "nextbutton", &err);
    UIUtilW::Assign(this, m_searchState, "searchstate");

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'MythSearchListDialog'");
        return false;
    }

    m_searchEdit->SetText(m_searchText);

    connect(m_searchEdit, &MythUITextEdit::valueChanged, this, &SearchButtonListDialog::searchChanged);
    connect(m_prevButton, &MythUIButton::Clicked, this, &SearchButtonListDialog::prevClicked);
    connect(m_nextButton, &MythUIButton::Clicked, this, &SearchButtonListDialog::nextClicked);

    BuildFocusList();

    return true;
}

bool SearchButtonListDialog::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions, false);

    for (int i = 0; i < actions.size() && !handled; ++i)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "0")
        {
            m_startsWith = !m_startsWith;
            searchChanged();
        }
        else
        {
            handled = false;
        }
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void SearchButtonListDialog::searchChanged(void)
{
    bool found = m_parentList->Find(m_searchEdit->GetText(), m_startsWith);

    if (m_searchState)
        m_searchState->DisplayState(found ? "found" : "notfound");
}

void SearchButtonListDialog::nextClicked(void)
{
    bool found = m_parentList->FindNext();

    if (m_searchState)
        m_searchState->DisplayState(found ? "found" : "notfound");
}

void SearchButtonListDialog::prevClicked(void)
{
    bool found = m_parentList->FindPrev();

    if (m_searchState)
        m_searchState->DisplayState(found ? "found" : "notfound");
}

void MythUIButtonList::SetScrollBarPosition()
{
    if (m_clearing || !m_scrollBar || !m_showScrollBar)
        return;

    int maximum = (m_itemCount <= m_itemsVisible) ? 0 : m_itemCount;
    m_scrollBar->SetMaximum(maximum);
    m_scrollBar->SetPageStep(m_itemsVisible);
    m_scrollBar->SetSliderPosition(m_selPosition);
    m_scrollBar->MoveToTop();
}
