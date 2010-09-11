
#include "mythuibuttonlist.h"

#include <math.h>

// QT headers
#include <QDomDocument>

// libmyth headers
#include "mythverbose.h"

// mythui headers
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
    m_arrange     = ArrangeFixed;
    m_alignment   = Qt::AlignLeft | Qt::AlignTop;
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
    m_maxVisible       = 0;
    m_columns          = 0;
    m_rows             = 0;
    m_leftColumns      = 0;
    m_rightColumns     = 0;
    m_topRows          = 0;
    m_bottomRows       = 0;
    m_lcdTitle         = "";

    m_upArrow = m_downArrow = NULL;

    m_buttontemplate = NULL;

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

/*!
 * \copydoc MythUIType::Reset()
 */
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

/*
 * The "width" of a button determines it relative position when using
 * Dynamic-Layout.
 *
 * If a button has a negative offset, that offset needs accounted for
 * to position the button in proper releation to the surrounding buttons.
 */
int MythUIButtonList::minButtonWidth(const MythRect & area)
{
    int width = area.width();

    /*
     * Assume if an overlap is allowed on the left, the same overlap
     * is on the right
     */
    if (area.x() < 0)
        width += (area.x() * 2 - 1); // x is negative
    else
        width += area.x();

    while (width < 0)
        width -= area.x(); // Oops

    return width;
}

/*
 * The "height" of a button determines it relative position when using
 * Dynamic-Layout.
 *
 * If a button has a negative offset, that offset needs accounted for
 * to position the button in proper releation to the surrounding buttons.
 */
int MythUIButtonList::minButtonHeight(const MythRect & area)
{
    int height = area.height();

    /*
     * Assume if an overlap is allowed on the top, the same overlap
     * is on the bottom
     */
    if (area.y() < 0)
        height += (area.y() * 2 - 1);
    else
        height += area.y();

    while (height < 0)
        height -= area.y(); // Oops

    return height;
}

/*
 * For Dynamic-Layout, buttons are allocated as needed.  If the list is
 * being redrawn, re-use any previously allocated buttons.
 */
MythUIGroup *MythUIButtonList::PrepareButton(int buttonIdx, int itemIdx,
                                             int & selectedIdx,
                                             int & button_shift)
{
    MythUIStateType *realButton;
    MythUIGroup *buttonstate;
    MythUIButtonListItem* buttonItem = m_itemList[itemIdx];

    buttonIdx += button_shift;
    if (buttonIdx < 0 || buttonIdx + 1 > m_maxVisible)
    {
        QString name = QString("buttonlist button %1").arg(m_maxVisible);
        MythUIStateType *button = new MythUIStateType(this, name);
        button->CopyFrom(m_buttontemplate);
        if (buttonIdx < 0)
        {
            /*
             * If a new button is needed in the front of the list, previously
             * existing buttons need shifted to the right.
             */
            m_ButtonList.prepend(button);
            buttonIdx = 0;
            ++button_shift;
            if (selectedIdx >= 0)
                ++selectedIdx;
        }
        else
            m_ButtonList.append(button);
        ++m_maxVisible;
    }

    realButton = m_ButtonList[buttonIdx];
    m_ButtonToItem[buttonIdx] = buttonItem;
    buttonItem->SetToRealButton(realButton, itemIdx == m_selPosition);
    buttonstate = dynamic_cast<MythUIGroup *>(realButton->GetCurrentState());

    if (itemIdx == m_selPosition)
        selectedIdx = buttonIdx;

    return buttonstate;
}

/*
 * Dynamically layout the buttons on a row.
 */
bool MythUIButtonList::DistributeRow(int & first_button, int & last_button,
                                     int & first_item, int & last_item,
                                     int & selected_column,
                                     bool grow_left, bool grow_right,
                                     int ** col_widths, int & row_height,
                                     int total_height, int split_height,
                                     int & col_cnt, bool & wrapped)
{
    MythUIGroup *buttonstate;
    int  initial_first_button, initial_last_button;
    int  initial_first_item,   initial_last_item;
    int  col_idx, left_cnt = 0;
    int  width, height;
    int  max_width, max_height;
    int  left_width, right_width;
    int  begin, end;
    bool underflow;
    bool added;
    bool hsplit, vsplit;
    int  selectedIdx;
    int  button_shift;

    selectedIdx = -1;
    button_shift = 0;
    col_cnt = 1;

    if (last_item + 1 > m_itemCount || last_item < 0 || first_item < 0)
        return false;

    /*
     * Allocate a button on the row.  With a vertical layout, there is
     * only one button per row, and this would be it.
     */
    if (grow_right)
        buttonstate = PrepareButton(last_button, last_item,
                                    selectedIdx, button_shift);
    else
        buttonstate = PrepareButton(first_button, first_item,
                                    selectedIdx, button_shift);

    if (buttonstate == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to query buttonlist state: %1")
                .arg(last_button));
        return false;
    }

    // Note size of initial button.
    max_width  = m_contentsRect.width();
    max_height = m_contentsRect.height();
    row_height = minButtonHeight(buttonstate->GetArea());
    width      = minButtonWidth(buttonstate->GetArea());

    /*
     * If the selected button should be centered, don't allow new buttons
     * to take up more than half the allowed area.
     */
    vsplit = (m_scrollStyle == ScrollCenter);
    hsplit = vsplit && grow_left && grow_right;
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
     * Otherwide, If adding a button to a column would exceed the
     * parent height, abort
    */
    if (total_height > 0 &&
        ((vsplit ? split_height : total_height) +
         m_itemVertSpacing + row_height > max_height))
    {
        VERBOSE(VB_GENERAL|VB_EXTRA,
                QString("%1 Height exceeded %2 + (%3) + %4 = %5 "
                        "which is > %6")
                .arg(vsplit ? "Centering" : "Total")
                .arg(split_height).arg(m_itemVertSpacing).arg(row_height)
                .arg(split_height + m_itemVertSpacing + row_height)
                .arg(max_height));
        first_button += button_shift;
        last_button += button_shift;
        return false;
    }

    VERBOSE(VB_GENERAL|VB_EXTRA, QString("Added button item %1 "
                                         "width %2 height %3")
            .arg(grow_right ? last_item : first_item)
            .arg(width).arg(row_height));

    initial_first_button = first_button;
    initial_last_button  = last_button;
    initial_first_item   = first_item;
    initial_last_item    = last_item;

    /*
     * if col_widths is not NULL, then grow_left & grow_right
     * are mutually exclusive.  So, col_idx can be anchored from
     * the left or right.
    */
    if (grow_right)
        col_idx = 0;
    else
        col_idx = m_columns - 1;

    // Add butons until no more fit.
    added = (m_layout != LayoutVertical);
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
                    end = m_itemCount;
            }

            if (last_item + 1 < end)
            {
                // Allocate next button to the right.
                buttonstate = PrepareButton(last_button + 1, last_item + 1,
                                            selectedIdx, button_shift);
                if (buttonstate == NULL)
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
                    VERBOSE(VB_GENERAL|VB_EXTRA,
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
                    height = minButtonHeight(buttonstate->GetArea());
                    if (row_height < height)
                        row_height = height;
                    VERBOSE(VB_GENERAL|VB_EXTRA,
                            QString("Added button item %1 "
                                    "r.width %2 height %3 total width %4+%5")
                            .arg(last_item).arg(width).arg(height)
                            .arg(left_width).arg(right_width));
                }
            }
            else
                underflow = true;
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
                    end = 0;
            }

            if (first_item > end)
            {
                buttonstate = PrepareButton(first_button - 1, first_item - 1,
                                            selectedIdx, button_shift);
                if (buttonstate == NULL)
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
                    VERBOSE(VB_GENERAL|VB_EXTRA,
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
                    ++left_cnt;
                    left_width += m_itemHorizSpacing + width;
                    height = minButtonHeight(buttonstate->GetArea());
                    if (row_height < height)
                        row_height = height;
                    VERBOSE(VB_GENERAL|VB_EXTRA,
                            QString("Added button item %1 "
                                    "l.width %2 height %3 total width %4+%5")
                            .arg(first_item).arg(width).arg(height)
                            .arg(left_width).arg(right_width));
                }
            }
            else
                underflow = true;
        }
    }

    /*
     * If total_height == 0, then this is the first row, so allow any height.
     * Otherwide, If adding a button to a column would exceed the
     * parent height, abort
    */
    if (total_height > 0 &&
        ((vsplit ? split_height : total_height) +
         m_itemVertSpacing + row_height > max_height))
    {
        VERBOSE(VB_GENERAL|VB_EXTRA,
                QString("%1 Height exceeded %2 + (%3) + %4 = %5 "
                        "which is > %6")
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

    if (*col_widths == 0)
    {
        /*
         * Allocate array to hold columns widths, now that we know
         * how many columns there are.
         */
        *col_widths = new int[col_cnt];
        for (col_idx = 0; col_idx < col_cnt; ++col_idx)
            (*col_widths)[col_idx] = 0;

    }

    // Adjust for insertions on the front.
    first_button += button_shift;
    last_button += button_shift;

    // It fits, so so note max column widths
    MythUIStateType *realButton;
    int buttonIdx;

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
        realButton = m_ButtonList[buttonIdx];
        buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());
        width = minButtonWidth(buttonstate->GetArea());
        if ((*col_widths)[col_idx] < width)
            (*col_widths)[col_idx] = width;

        // Make note of which column as the selected button
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
bool MythUIButtonList::DistributeCols(int & first_button, int & last_button,
                                      int & first_item, int & last_item,
                                      int & selected_column, int & selected_row,
                                      int ** col_widths,
                                      QList<int> & row_heights,
                                      int & top_height, int & bottom_height,
                                      bool & wrapped)
{
    int  col_cnt;
    int  height;
    int  row_cnt = 1;
    int  end;
    bool added;

    do
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
                end = m_itemCount;
        }

        if (last_item + 1 < end)
        {
            // Does another row fit?
            if (DistributeRow(first_button, ++last_button,
                              first_item, ++last_item, selected_column,
                              false, true, col_widths, height,
                              top_height + bottom_height, bottom_height,
                              col_cnt, wrapped))
            {
                if (col_cnt < m_columns)
                    return false;  // Need to try again with fewer cols

                if (selected_row == -1 && selected_column != -1)
                    selected_row = row_heights.size();
                ++row_cnt;
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
                end = 0;
        }

        if (first_item > end)
        {
            // Can we insert another row?
            if (DistributeRow(--first_button, last_button,
                              --first_item, last_item, selected_column,
                              true, false, col_widths, height,
                              top_height + bottom_height, top_height,
                              col_cnt, wrapped))
            {
                if (col_cnt < m_columns)
                    return false;  // Need to try again with fewer cols

                if (selected_row == -1 && selected_column != -1)
                    selected_row = row_heights.size();
                else if (selected_row != -1)
                    ++selected_row;
                ++row_cnt;
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
    } while (added);

    return true;
}

/*
 * Dynamically layout as many buttons as will fit in the area.
 */
bool MythUIButtonList::DistributeButtons(void)
{
    int  first_button, last_button, start_button, start_item;
    int  first_item, last_item;
    int *col_widths;
    int  col_cnt;
    int  selected_column, selected_row;
    bool wrapped = false;
    bool grow_left = true;

    QList<int> row_heights;
    QList<int>::iterator Iheight;
    int height, top_height, bottom_height;

    start_item = m_selPosition;
    selected_column = selected_row = -1;
    top_height = bottom_height = 0;
    col_widths = 0;
    first_button = last_button = start_button = 0;

    VERBOSE(VB_GENERAL|VB_EXTRA, QString("DistributeButtons: "
                                         "selected item %1 total items %2")
            .arg(start_item).arg(m_itemCount));

    /*
     * Try fewer and fewer columns until each row can fit the same
     * number of columns.
     */
    for (m_columns = m_itemCount; m_columns > 0; )
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
            start_button = qMax((m_maxVisible / 2) - 1, 0);
            break;
          case ScrollFree:
            if (m_layout == LayoutGrid)
            {
                start_button = 0;
                first_item = last_item = 0;
                grow_left = false;
            }
            else if (!m_ButtonList.empty())
            {
                if (m_itemCount - m_selPosition - 1 <
                    static_cast<int>(m_ButtonList.size()) / 2)
                    start_button = m_ButtonList.size() -
                                   (m_itemCount - m_selPosition) + 1;
                else if (m_selPosition >
                         static_cast<int>(m_ButtonList.size()) / 2)
                    start_button = (m_ButtonList.size() / 2);
                else
                    start_button = m_selPosition;
            }
            else
                start_button = 0;
            break;
        }

        first_button = last_button = start_button;
        row_heights.clear();

        // Process row with selected button, and set starting val for m_columns.
        if (!DistributeRow(first_button, last_button,
                           first_item, last_item, selected_column,
                           grow_left, true, &col_widths,
                           height, 0, 0, col_cnt, wrapped))
            return false;
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
            start_button = qMax((int)m_ButtonList.size() / 2, 0);
            start_button = (start_button / qMax(m_columns, 1)) * m_columns;
            if (start_button < m_itemCount / 2 &&
                m_itemCount - m_selPosition - 1 < (int)m_ButtonList.size() / 2)
                start_button += m_columns;
            first_button = last_button = start_button;

            // Now do initial row layout again with our new knowledge
            selected_column = selected_row = -1;
            if (!DistributeRow(first_button, last_button,
                               first_item, last_item, selected_column,
                               grow_left, true, &col_widths,
                               height, 0, 0, col_cnt, wrapped))
                return false;
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
                           &col_widths, row_heights,
                           top_height, bottom_height, wrapped))
            break; // Buttons fit on each row, so done

        delete[] col_widths;
        col_widths = 0;

        --m_columns;
        start_item = m_selPosition;
    }

    m_rows = row_heights.size();

    VERBOSE(VB_GENERAL|VB_EXTRA,
            QString("%1 rows, %2 columns fit inside parent area %3x%4")
            .arg(m_rows).arg(m_columns).arg(m_contentsRect.width())
            .arg(m_contentsRect.height()));

    if (col_widths == 0)
        return false;

    int total, row, col;
    int left_spacing, right_spacing, top_spacing, bottom_spacing;
    int x, y, x_init, x_adj, y_adj;
    QString status_msg;

    /*
     * Calculate heights of buttons on each side of selected button
     */
    top_height = bottom_height = m_topRows = m_bottomRows = 0;

    status_msg = "Row heights: ";
    for (row = 0; row < m_rows; ++row)
    {
        if ( row != 0)
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
    y = m_contentsRect.y();
    if (m_alignment & Qt::AlignVCenter && m_arrange != ArrangeFill)
    {
        if (m_scrollStyle == ScrollCenter)
        {
            // Adjust to compensate for top height less than bottom height
            y += qMax(bottom_height - top_height, 0);
            total = qMax(top_height, bottom_height) * 2;
        }
        else
            total = top_height + bottom_height;

        // Adjust top margin so selected button ends up in the middle
        y += (qMax(m_contentsRect.height() - total, 2) / 2);
    }
    else if (m_alignment & Qt::AlignBottom && m_arrange == ArrangeStack)
    {
        // Adjust top margin so buttons are bottom justified
        y += qMax(m_contentsRect.height() -
                  (top_height + bottom_height), 0);
    }

    status_msg += QString(" spacing top %1 bottom %2 fixed %3 offset %4")
                  .arg(top_spacing).arg(bottom_spacing)
                  .arg(m_itemVertSpacing).arg(y);

    VERBOSE(VB_GENERAL|VB_EXTRA, status_msg);

    /*
     * Calculate width of buttons on each side of selected button
     */
    int left_width, right_width;

    left_width = right_width = m_leftColumns = m_rightColumns = 0;

    status_msg = "Col widths: ";
    for (col = 0; col < m_columns; ++col)
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
    x_init = m_contentsRect.x();
    if (m_alignment & Qt::AlignHCenter && m_arrange != ArrangeFill)
    {
        if (m_scrollStyle == ScrollCenter)
        {
            // Compensate for left being smaller than right
            x_init += qMax(right_width - left_width, 0);
            total = qMax(left_width, right_width) * 2;
        }
        else
            total = left_width + right_width;

        // Adjust left margin so selected button ends up in the middle
        x_init += (qMax(m_contentsRect.width() - total, 2) / 2);
    }
    else if (m_alignment & Qt::AlignRight && m_arrange == ArrangeStack)
    {
        // Adjust left margin, so buttons are right justified
        x_init += qMax(m_contentsRect.width() -
                       (left_width + right_width), 0);
    }

    status_msg += QString(" spacing left %1 right %2 fixed %3 offset %4")
                  .arg(left_spacing).arg(right_spacing)
                  .arg(m_itemHorizSpacing).arg(x_init);
    VERBOSE(VB_GENERAL|VB_EXTRA, status_msg);

    top_spacing    += m_itemVertSpacing;
    bottom_spacing += m_itemVertSpacing;
    left_spacing   += m_itemHorizSpacing;
    right_spacing  += m_itemHorizSpacing;

    MythUIStateType *realButton = NULL;
    MythUIGroup *buttonstate;

    // Calculate position of each button
    int vertical_spacing, horizontal_spacing;
    int buttonIdx = first_button;

    vertical_spacing = top_spacing;
    for (row = 0; row < m_rows; ++row)
    {
        x = x_init;
        horizontal_spacing = left_spacing;
        for (col = 0; col < m_columns && buttonIdx <= last_button; ++col)
        {
            realButton = m_ButtonList[buttonIdx];
            buttonstate = dynamic_cast<MythUIGroup *>
                          (realButton->GetCurrentState());

            // Center button within width of column
            if (m_alignment & Qt::AlignHCenter)
                x_adj = (col_widths[col] -
                         minButtonWidth(buttonstate->GetArea())) / 2;
            else if (m_alignment & Qt::AlignRight)
                x_adj = (col_widths[col] -
                         minButtonWidth(buttonstate->GetArea()));
            else
                x_adj = 0;

            // Center button within height of row.
            if (m_alignment & Qt::AlignVCenter)
                y_adj = (row_heights[row] -
                         minButtonHeight(buttonstate->GetArea())) / 2;
            else if (m_alignment & Qt::AlignBottom)
                y_adj = (row_heights[row] -
                         minButtonHeight(buttonstate->GetArea()));
            else
                y_adj = 0;

            // Set position of button
            realButton->SetPosition(x + x_adj, y + y_adj);
            realButton->SetVisible(true);

            if (col == selected_column)
                horizontal_spacing = right_spacing;

            x += col_widths[col] + horizontal_spacing;
            ++buttonIdx;
        }
        if (row == selected_row)
            vertical_spacing = bottom_spacing;

        y += row_heights[row] + vertical_spacing;
    }

    m_itemsVisible = m_columns * m_rows;

    // Hide buttons before first active button
    for (buttonIdx = 0; buttonIdx < first_button; ++buttonIdx)
        m_ButtonList[buttonIdx]->SetVisible(false);
    // Hide buttons after last active buttons.
    for (buttonIdx = m_maxVisible - 1; buttonIdx > last_button; --buttonIdx)
        m_ButtonList[buttonIdx]->SetVisible(false);

    // Set m_topPosition so arrows are displayed correctly.
    if (m_scrollStyle == ScrollCenter || m_scrollStyle == ScrollGroupCenter)
        m_topPosition = static_cast<int>(m_itemsVisible) < m_itemCount;
    else
        m_topPosition = first_item;

    delete[] col_widths;
    return true;
}

void MythUIButtonList::SetPosition(void)
{
    if (m_ButtonList.size() == 0)
        return;

    int button = 0;

    switch (m_scrollStyle)
    {
      case ScrollCenter:
      case ScrollGroupCenter:
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

    if (m_scrollStyle == ScrollCenter || m_scrollStyle == ScrollGroupCenter)
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

void MythUIButtonList::SanitizePosition(void)
{
    if (m_selPosition < 0)
        m_selPosition = (m_wrapStyle > WrapNone) ? m_itemList.size() - 1 : 0;
    else if (m_selPosition >= m_itemList.size())
        m_selPosition = (m_wrapStyle > WrapNone) ? 0 : m_itemList.size() - 1;
}

void MythUIButtonList::SetPositionArrowStates()
{
    if (!m_initialized)
        Init();

    if (!m_initialized)
        return;

    if (m_clearing)
        return;

    m_needsUpdate = false;

    // set topitem, top position
    SanitizePosition();
    m_ButtonToItem.clear();

    if (m_arrange == ArrangeFixed)
        SetPosition();
    else
        DistributeButtons();

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

void MythUIButtonList::ItemVisible(MythUIButtonListItem* item)
{
    if (item)
        emit itemVisible(item);
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

uint MythUIButtonList::GetVisibleCount()
{
    if (m_needsUpdate)
        SetPositionArrowStates();

    return m_itemsVisible;
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
    if (pos < 0 || pos >= m_itemList.size())
        return NULL;

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

void MythUIButtonList::InitButton(int itemIdx, MythUIStateType* & realButton,
                                  MythUIButtonListItem* & buttonItem)
{
    buttonItem = m_itemList[itemIdx];

    if (m_maxVisible == 0)
    {
        QString name("buttonlist button 0");
        MythUIStateType *button = new MythUIStateType(this, name);
        button->CopyFrom(m_buttontemplate);
        m_ButtonList.append(button);
        ++m_maxVisible;
    }

    realButton = m_ButtonList[0];
    m_ButtonToItem[0] = buttonItem;
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
    MythUIGroup*     buttonstate;
    MythUIStateType* realButton;
    MythUIButtonListItem* buttonItem;

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
        for ( ; pos >= 0; --pos)
        {
            InitButton(pos, realButton, buttonItem);
            buttonItem->SetToRealButton(realButton, true);
            buttonstate = dynamic_cast<MythUIGroup *>
                         (realButton->GetCurrentState());
            if (buttonstate == NULL)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("PageUp: Failed to query buttonlist state"));
                return pos;
            }
            if (total + m_itemHorizSpacing +
                buttonstate->GetArea().width() / 2 >= max_width)
                return pos + 1;

            buttonItem->SetToRealButton(realButton, false);
            buttonstate = dynamic_cast<MythUIGroup *>
                         (realButton->GetCurrentState());
            total += m_itemHorizSpacing + buttonstate->GetArea().width();
        }
        return 0;
    }

    // Grid or Vertical
    int dec;

    if (m_layout == LayoutGrid)
    {
        /*
         * Adjusting using bottomRow:TopRow only works if new page
         * has the same ratio as the previous page, but that is common
         * with the grid layout, so go for it.  If themers start doing
         * grids where this is not true, then this will need to be modified.
         */
        pos -= (m_columns * (m_topRows + 1 +
                             qMax(m_bottomRows - m_topRows, 0)));
        dec = m_columns;
    }
    else
    {
        pos -= (m_topRows + 1);
        dec = 1;
    }

    int max_height = m_contentsRect.height() / 2;
    for ( ; pos >= 0; pos -= dec)
    {
        InitButton(pos, realButton, buttonItem);
        buttonItem->SetToRealButton(realButton, true);
        buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());
        if (buttonstate == NULL)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("PageUp: Failed to query buttonlist state"));
            return pos;
        }
        if (total + m_itemHorizSpacing +
            buttonstate->GetArea().height() / 2 >= max_height)
            return pos + dec;

        buttonItem->SetToRealButton(realButton, false);
        buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());
        total += m_itemHorizSpacing + buttonstate->GetArea().height();
    }
    return 0;
}

int MythUIButtonList::PageDown(void)
{
    int pos        = m_selPosition;
    int num_items  = m_itemList.size();
    int total      = 0;
    MythUIGroup*     buttonstate;
    MythUIStateType* realButton;
    MythUIButtonListItem* buttonItem;

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
        for ( ; pos < num_items; ++pos)
        {
            InitButton(pos, realButton, buttonItem);
            buttonItem->SetToRealButton(realButton, true);
            buttonstate = dynamic_cast<MythUIGroup *>
                         (realButton->GetCurrentState());
            if (buttonstate == NULL)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("PageDown: Failed to query buttonlist state"));
                return pos;
            }
            if (total + m_itemHorizSpacing +
                buttonstate->GetArea().width() / 2 >= max_width)
                return pos - 1;

            buttonItem->SetToRealButton(realButton, false);
            buttonstate = dynamic_cast<MythUIGroup *>
                         (realButton->GetCurrentState());
            total += m_itemHorizSpacing + buttonstate->GetArea().width();
        }
        return num_items - 1;
    }

    // Grid or Vertical
    int inc;

    if (m_layout == LayoutGrid)
    {
        /*
         * Adjusting using bottomRow:TopRow only works if new page
         * has the same ratio as the previous page, but that is common
         * with the grid layout, so go for it.  If themers start doing
         * grids where this is not true, then this will need to be modified.
         */
        pos += (m_columns * (m_bottomRows + 1 +
                             qMax(m_topRows - m_bottomRows, 0)));
        inc = m_columns;
    }
    else
    {
        pos += (m_bottomRows + 1);
        inc = 1;
    }

    int max_height = m_contentsRect.height() / 2;
    for ( ; pos < num_items; pos += inc)
    {
        InitButton(pos, realButton, buttonItem);
        buttonItem->SetToRealButton(realButton, true);
        buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());
        if (buttonstate == NULL)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("PageDown: Failed to query buttonlist state"));
            return pos;
        }
        if (total + m_itemHorizSpacing +
            buttonstate->GetArea().height() / 2 >= max_height)
            return pos - inc;

        buttonItem->SetToRealButton(realButton, false);
        buttonstate = dynamic_cast<MythUIGroup *>
                      (realButton->GetCurrentState());
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
            break;
        case MoveColumn:
            if (pos % m_columns > 0)
                --m_selPosition;
            else if (m_wrapStyle == WrapFlowing)
                if (m_selPosition == 0)
                    --m_selPosition = m_itemList.size() - 1;
                else
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
            if (m_arrange == ArrangeFixed)
                m_selPosition = qMax(0, m_selPosition - (int)m_itemsVisible);
            else
                m_selPosition = PageUp();
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
            else if (m_wrapStyle == WrapFlowing)
                if (m_selPosition < m_itemList.size() - 1)
                    ++m_selPosition;
                else
                    m_selPosition = 0;
            else if (m_wrapStyle > WrapNone)
                m_selPosition = pos - (m_columns-1);
            else if (m_wrapStyle == WrapCaptive)
                return true;
            break;
        case MoveRow:
            if (((m_itemList.size()-1) / qMax(m_columns, 0) ) > (pos / m_columns))
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
            if (m_arrange == ArrangeFixed)
                m_selPosition = qMin(m_itemCount - 1,
                                     m_selPosition + (int)m_itemsVisible);
            else
                m_selPosition = PageDown();
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

    m_contentsRect.CalculateArea(m_Area);

    m_buttontemplate = dynamic_cast<MythUIStateType *>(GetChild("buttonitem"));

    if (!m_buttontemplate)
    {
        VERBOSE(VB_IMPORTANT, QString("Statetype buttonitem is required in "
                                      "mythuibuttonlist: %1")
                .arg(objectName()));
        return;
    }

    m_buttontemplate->SetVisible(false);

    if (m_arrange == ArrangeFixed)
    {

        /*
         * If fixed spacing is defined, then use the "active" state size
         * to predictively determine the position of each button.
         */
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

        CalculateVisibleItems();

        int col = 1;
        int row = 1;

        for (int i = 0; i < (int)m_itemsVisible; i++)
        {
            QString name = QString("buttonlist button %1").arg(i);
            MythUIStateType *button = new MythUIStateType(this, name);
            button->CopyFrom(m_buttontemplate);

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
        // This can be removed once the disk based image caching is added to
        // mythui, since the mythgallery thumbnail generator can be ditched.
        MythUIGroup *buttonSelectedState = dynamic_cast<MythUIGroup *>
                                   (m_buttontemplate->GetState("selected"));

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

/**
 *  \copydoc MythUIType::keyPressEvent()
 */
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
            {
                if (m_scrollStyle == ScrollFree)
                    handled = MoveDown(MoveColumn);
                else
                    handled = MoveDown(MoveItem);
            }
            else
                handled = false;
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

/**
 *  \copydoc MythUIType::gestureEvent()
 */
bool MythUIButtonList::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;

    if (event->gesture() == MythGestureEvent::Click)
    {
        // We want the relative position of the click
        QPoint position = event->GetPosition() - m_Parent->GetArea().topLeft();

        MythUIType *type = GetChildAt(position,false,false);

        if (!type)
            return false;

        MythUIStateType *object = dynamic_cast<MythUIStateType *>(type);
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
            else
                handled = false;
        }
    }

    return handled;
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
    m_itemsVisible = 0;
    m_rows = 0;
    m_columns = 0;

    if ((m_layout == LayoutHorizontal) || (m_layout == LayoutGrid))
    {
        int x = 0;
        while (x <= m_contentsRect.width() - m_itemWidth)
        {
            x += m_itemWidth + m_itemHorizSpacing;
            m_columns++;
        }
    }

    if ((m_layout == LayoutVertical) || (m_layout == LayoutGrid))
    {
        int y = 0;
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

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythUIButtonList::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
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
        m_alignment = parseAlignment(align);
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
        m_showArrow = parseBool(element);
    else if (element.tagName() == "spacing")
    {
        m_itemHorizSpacing = NormX(getFirstText(element).toInt());
        m_itemVertSpacing = NormY(getFirstText(element).toInt());
    }
    else if (element.tagName() == "drawfrombottom")
    {
        m_drawFromBottom = parseBool(element);
        m_alignment |= Qt::AlignBottom;
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
void MythUIButtonList::DrawSelf(MythPainter *, int, int, int, QRect)
{
    if (m_needsUpdate)
        SetPositionArrowStates();
}

/**
 *  \copydoc MythUIType::CreateCopy()
 */
void MythUIButtonList::CreateCopy(MythUIType *parent)
{
    MythUIButtonList *lb = new MythUIButtonList(parent, objectName());
    lb->CopyFrom(this);
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythUIButtonList::CopyFrom(MythUIType *base)
{
    MythUIButtonList *lb = dynamic_cast<MythUIButtonList *>(base);
    if (!lb)
        return;

    m_layout = lb->m_layout;
    m_arrange = lb->m_arrange;
    m_alignment = lb->m_alignment;

    m_contentsRect = lb->m_contentsRect;

    m_itemHeight = lb->m_itemHeight;
    m_itemWidth = lb->m_itemWidth;
    m_itemHorizSpacing = lb->m_itemHorizSpacing;
    m_itemVertSpacing = lb->m_itemVertSpacing;
    m_itemsVisible = lb->m_itemsVisible;
    m_maxVisible = lb->m_maxVisible;

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
        deltype = GetChild(name);
        delete deltype;
    }

    m_ButtonList.clear();

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

QString MythUIButtonListItem::GetText(const QString &name) const
{
    if (name.isEmpty())
        return m_text;
    else if (m_strings.contains(name))
        return m_strings[name].text;
    else
        return QString();
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

    QString state;
    if (selected)
    {
        button->MoveToTop();
        state = m_parent->m_active ? "selectedactive" : "selectedinactive";
    }
    else
        state = m_parent->m_active ? "active" : "inactive";

    if (!button->DisplayState(state) && state == "inactive")
    {
        state = "active";
        button->DisplayState(state);
    }

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

            QString newText = text->GetTemplateText();
            if (newText.isEmpty())
                newText = text->GetDefaultText();
            QRegExp regexp("%(\\|(.))?([^\\|]+)(\\|(.))?%");
            regexp.setMinimal(true);
            if (newText.contains(regexp))
            {
                int pos = 0;
                QString tempString = newText;
                while ((pos = regexp.indexIn(newText, pos)) != -1)
                {
                    QString key = regexp.cap(3).toLower().trimmed();
                    QString replacement;
                    QString value = m_strings.value(key).text;
                    if (!value.isEmpty())
                    {
                        replacement = QString("%1%2%3")
                                                .arg(regexp.cap(2))
                                                .arg(m_strings.value(key).text)
                                                .arg(regexp.cap(5));
                    }
                    tempString.replace(regexp.cap(0), replacement);
                    pos += regexp.matchedLength();
                }
                newText = tempString;
            }
            else
                newText = textprop.text;

            text->SetText(newText);
            text->SetFontState(textprop.state.isEmpty() ? m_fontState : textprop.state);
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

    m_parent->ItemVisible(this);
}
