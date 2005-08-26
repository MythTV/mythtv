#include <qapplication.h>
#include <qpainter.h>
#include <qpixmap.h>

#include <cassert>

#include "mythcontext.h"
#include "mythlistbutton.h"
#include "mythmainwindow.h"
#include "mythcontext.h"
#include "mythfontproperties.h"
#include "mythuistatetype.h"
#include "mythuibutton.h"

MythListButton::MythListButton(MythUIType *parent, const char *name, 
                               const QRect& area, bool showArrow, 
                               bool showScrollArrows)
              : MythUIType(parent, name)
{
    m_Area             = area;

    m_showArrow        = showArrow;
    m_showScrollArrows = showScrollArrows;

    m_active           = false;
    m_drawFromBottom   = false;

    m_itemList.setAutoDelete(false);
    m_topItem = 0;
    m_selItem = 0;
    m_selIterator = new QPtrListIterator<MythListButtonItem>(m_itemList);
    m_topIterator = new QPtrListIterator<MythListButtonItem>(m_itemList);
    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount = 0;

    m_initialized     = false;
    m_clearing        = false;
    m_itemSpacing     = 0;
    m_itemMargin      = 0;
    m_itemHeight      = 0;
    m_itemsVisible    = 0;

    m_upArrow = m_downArrow = NULL;

    m_textFlags = Qt::AlignLeft | Qt::AlignVCenter;

    SetItemRegColor(QColor("#505050"), QColor("#000000"), 100);
    SetItemSelColor(QColor("#52CA38"), QColor("#349838"), 255);

    m_fontActive = new MythFontProperties();
    m_fontInactive = new MythFontProperties();
}

MythListButton::~MythListButton()
{    
    Reset();
    delete m_topIterator;
    delete m_selIterator;

    if (m_fontActive)
       delete m_fontActive; 
    if (m_fontInactive)
       delete m_fontInactive; 
}

void MythListButton::SetItemRegColor(const QColor& beg, 
                                     const QColor& end,
                                     uint alpha)
{
    m_itemRegBeg   = beg;
    m_itemRegEnd   = end;
    m_itemRegAlpha = alpha;
}

void MythListButton::SetItemSelColor(const QColor& beg,
                                     const QColor& end,
                                     uint alpha)
{
    m_itemSelBeg   = beg;
    m_itemSelEnd   = end;
    m_itemSelAlpha = alpha;
}

void MythListButton::SetFontActive(const MythFontProperties &font)
{
    *m_fontActive   = font;
}

void MythListButton::SetFontInactive(const MythFontProperties &font)
{
    *m_fontInactive = font;
}

void MythListButton::SetSpacing(int spacing)
{
    m_itemSpacing = spacing;    
}

void MythListButton::SetMargin(int margin)
{
    m_itemMargin = margin;    
}

void MythListButton::SetDrawFromBottom(bool draw)
{
    m_drawFromBottom = draw;
}

void MythListButton::SetTextFlags(int flags)
{
    m_textFlags = flags;
}

void MythListButton::SetActive(bool active)
{
    m_active = active;
    if (m_initialized)
        SetPositionArrowStates();
}

void MythListButton::Reset()
{
    m_clearing = true;

    MythListButtonItem* item = 0;
    for (item = m_itemList.first(); item; item = m_itemList.next())
        delete item;

    m_clearing = false;
    m_itemList.clear();
    
    m_topItem     = 0;
    m_selItem     = 0;
    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount   = 0;
    m_selIterator->toFirst(); 
    m_topIterator->toFirst();

    SetPositionArrowStates();
}

void MythListButton::SetPositionArrowStates(void)
{
    if (!m_initialized)
        Init();

    if (m_ButtonList.size() > 0)
    {
        int button = 0;
    
        if (m_drawFromBottom && m_itemCount < (int)m_itemsVisible)
            button = m_itemsVisible - m_itemCount;

        for (int i = 0; i < button; i++)
             m_ButtonList[i]->SetVisible(false);

        QPtrListIterator<MythListButtonItem> it = (*m_topIterator);
        while (it.current() && button < (int)m_itemsVisible)
        {
            MythUIButton *realButton = m_ButtonList[button];
            MythListButtonItem *buttonItem = it.current();

            buttonItem->SetToRealButton(realButton, true);
            realButton->SetVisible(true);

            button++;
            ++it;
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

void MythListButton::InsertItem(MythListButtonItem *item)
{
    MythListButtonItem* lastItem = m_itemList.last();
    m_itemList.append(item);

    m_itemCount++;

    if (!lastItem) 
    {
        m_topItem = item;
        m_selItem = item;
        m_selIterator->toFirst();
        m_topIterator->toFirst();
        m_selPosition = m_topPosition = 0;
        emit itemSelected(item);
    }

    SetPositionArrowStates();
}

void MythListButton::RemoveItem(MythListButtonItem *item)
{
    if (m_clearing)
        return;
    
    if (m_itemList.findRef(item) == -1)
        return;

    if (item == m_topItem)
    {
        if (m_topItem != m_itemList.last())
        {
            ++(*m_topIterator);
            ++m_topPosition;
            m_topItem = m_topIterator->current();
        }
        else if (m_topItem != m_itemList.first())
        {
            --(*m_topIterator);
            --m_topPosition;
            m_topItem = m_topIterator->current();
        }
        else
        {
            m_topItem = NULL;
            m_topPosition = 0;
            m_topIterator->toFirst();
        }
    }

    if (item == m_selItem)
    {
        if (m_selItem != m_itemList.last())
        {
            ++(*m_selIterator);
            ++m_selPosition;
            m_selItem = m_selIterator->current();
        }
        else if (m_selItem != m_itemList.first())
        {
            --(*m_selIterator);
            --m_selPosition;
            m_selItem = m_selIterator->current();
        }
        else
        {
            m_selItem = NULL;
            m_selPosition = 0;
            m_selIterator->toFirst();
        }
    }

    m_itemList.remove(item);
    m_itemCount--;

    SetPositionArrowStates();

    if (m_selItem) 
        emit itemSelected(m_selItem);
}

void MythListButton::SetItemCurrent(MythListButtonItem* item)
{
    m_selIterator->toFirst();
    MythListButtonItem *cur;
    bool found = false;
    m_selPosition = 0;
    while ((cur = m_selIterator->current()) != 0)
    {
        if (cur == item)
        {
            found = true;
            break;
        }

        ++(*m_selIterator);
        ++m_selPosition;
    }

    if (!found)
    {
        m_selIterator->toFirst();
        m_selPosition = 0;
    }

    m_itemCount = m_selPosition;
    m_topItem = item;
    m_selItem = item;
    (*m_topIterator) = (*m_selIterator);

    SetPositionArrowStates();

    emit itemSelected(m_selItem);
}

void MythListButton::SetItemCurrent(int current)
{
    MythListButtonItem* item = m_itemList.at(current);
    if (!item)
        item = m_itemList.first();

    SetItemCurrent(item);
}

MythListButtonItem* MythListButton::GetItemCurrent()
{
    return m_selItem;
}

MythListButtonItem* MythListButton::GetItemFirst()
{
    return m_itemList.first();    
}

MythListButtonItem* MythListButton::GetItemNext(MythListButtonItem *item)
{
    if (m_itemList.findRef(item) == -1)
        return 0;

    return m_itemList.next();
}

int MythListButton::GetCount()
{
    return m_itemCount;
}

QPtrListIterator<MythListButtonItem> MythListButton::GetIterator(void)
{
    return QPtrListIterator<MythListButtonItem>(m_itemList);
}

MythListButtonItem* MythListButton::GetItemAt(int pos)
{
    return m_itemList.at(pos);    
}

int MythListButton::GetItemPos(MythListButtonItem* item)
{
    if (!item)
        return -1;
    return m_itemList.findRef(item);    
}

void MythListButton::MoveUp(MovementUnit unit)
{
    int pos = m_selPosition;
    if (pos == -1)
        return;

    switch (unit)
    {
        case MoveItem:
            if (!m_selIterator->atFirst())
            {
                --(*m_selIterator);
                --m_selPosition;
            }
            break;
        case MovePage:
            if (pos > (int)m_itemsVisible)
            {
                for (int i = 0; i < (int)m_itemsVisible; i++)
                {
                    --(*m_selIterator);
                    --m_selPosition;
                }
                break;
            }
            // fall through
        case MoveMax:
            m_selIterator->toFirst();
            m_selPosition = 0;
            break;
    }

    if (!m_selIterator->current())
        return;

    m_selItem = m_selIterator->current();

    if (m_selPosition <= m_topPosition)
    {
        m_topItem = m_selItem;
        (*m_topIterator) = (*m_selIterator);
        m_topPosition = m_selPosition;
    }

    SetPositionArrowStates();

    emit itemSelected(m_selItem);
}

void MythListButton::MoveDown(MovementUnit unit)
{
    int pos = m_selPosition;
    if (pos == -1)
        return;

    switch (unit)
    {
        case MoveItem:
            if (!m_selIterator->atLast())
            {
                ++(*m_selIterator);
                ++m_selPosition;
            }
            break;
        case MovePage:
            if ((pos + (int)m_itemsVisible) < m_itemCount - 1)
            {
                for (int i = 0; i < (int)m_itemsVisible; i++)
                {
                    ++(*m_selIterator);
                    ++m_selPosition;
                }
                break;
            }
            // fall through
        case MoveMax:
            m_selIterator->toLast();
            m_selPosition = m_itemCount - 1;
            break;
    }

    if (!m_selIterator->current()) 
        return;

    m_selItem = m_selIterator->current();

    while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1) 
    {
        ++(*m_topIterator);
        ++m_topPosition;
    }
   
    m_topItem = m_topIterator->current();

    SetPositionArrowStates();

    emit itemSelected(m_selItem);
}

bool MythListButton::MoveToNamedPosition(const QString &position_name)
{
    if(m_selPosition < 0)
    {
        return false;
    }

    if(!m_selIterator->toFirst())
    {
        return false;
    }
    m_selPosition = 0;
    
    bool found_it = false;
    while(m_selIterator->current())
    {
        if(m_selIterator->current()->text() == position_name)
        {
            found_it = true;
            break;
        }
        ++(*m_selIterator);
        ++m_selPosition;
    }

    if(!found_it)
    {
        m_selPosition = -1;
        return false;
    }

    m_selItem = m_selIterator->current();

    while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1) 
    {
        ++(*m_topIterator);
        ++m_topPosition;
    }
   
    m_topItem = m_topIterator->current();

    SetPositionArrowStates();

    return true;
}

bool MythListButton::MoveItemUpDown(MythListButtonItem *item, bool flag)
{
    if (item != m_selItem)
    {
        cerr << "Can't move non-selected item\n";
        return false;
    }

    if (item == m_itemList.getFirst() && flag)
        return false;
    if (item == m_itemList.getLast() && !flag)
        return false;

    int oldpos = m_selPosition;
    int insertat = 0;
    bool dolast = false;

    if (flag)
    {
        insertat = m_selPosition - 1;
        if (item == m_itemList.getLast())
            dolast = true;
        else
            ++m_selPosition;

        if (item == m_topItem)
            ++m_topPosition;
    }
    else
        insertat = m_selPosition + 1;

    if (m_itemList.current() == item)
    {
        m_itemList.take();
        //  cout << "speedy\n";
    }
    else
        m_itemList.take(oldpos);

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

void MythListButton::Init()
{
    if (m_initialized)
        return;

    m_initialized = true;

    QFontMetrics fm(m_fontActive->face);
    QSize sz1 = fm.size(Qt::SingleLine, "XXXXX");
    fm = QFontMetrics(m_fontInactive->face);
    QSize sz2 = fm.size(Qt::SingleLine, "XXXXX");
    m_itemHeight = QMAX(sz1.height(), sz2.height()) + (int)(2 * m_itemMargin);

    QRect arrowsRect;
    if (m_showScrollArrows) 
    {
        m_upArrow = new MythUIStateType(this, "upscrollarrow");
        m_downArrow = new MythUIStateType(this, "downscrollarrow");

        MythImage *upReg, *upAct, *downReg, *downAct;
        LoadArrowPixmaps(&upReg, &upAct, &downReg, &downAct);

        m_upArrow->AddImage(MythUIStateType::Off, upReg);
        m_upArrow->AddImage(MythUIStateType::Full, upAct);
        m_downArrow->AddImage(MythUIStateType::Off, downReg);
        m_downArrow->AddImage(MythUIStateType::Full, downAct);

        arrowsRect = PlaceArrows(upAct->size());

        upAct->DownRef();
        upReg->DownRef();
        downReg->DownRef();
        downAct->DownRef();
    }
    else 
        arrowsRect = QRect(0, 0, 0, 0);
 
    m_contentsRect = CalculateContentsRect(arrowsRect);

    CalculateVisibleItems();

    MythImage *itemRegPix, *itemSelActPix, *itemSelInactPix;
    MythImage *arrowPix, *checkNonePix, *checkHalfPix, *checkFullPix;

    LoadPixmap(&checkNonePix, "check-empty");
    LoadPixmap(&checkHalfPix, "check-half");
    LoadPixmap(&checkFullPix, "check-full");

    LoadPixmap(&arrowPix, "arrow");

    QSize gradsize(ItemWidth(), m_itemHeight);
    itemRegPix = MythImage::Gradient(gradsize, m_itemRegBeg, m_itemRegEnd,
                                     m_itemRegAlpha);
    itemSelActPix = MythImage::Gradient(gradsize, m_itemSelBeg, m_itemSelEnd,
                                        m_itemSelAlpha);
    itemSelInactPix = MythImage::Gradient(gradsize, m_itemSelBeg, m_itemSelEnd,
                                          m_itemRegAlpha);

    for (int i = 0; i < (int)m_itemsVisible; i++)
    {
        QString name = QString("buttonlist button %1").arg(i);
        MythUIButton *button = new MythUIButton(this, name);
        button->SetBackgroundImage(MythUIButton::Normal, itemRegPix);
        button->SetBackgroundImage(MythUIButton::Active, itemRegPix);
        button->SetBackgroundImage(MythUIButton::SelectedInactive,
                                   itemSelInactPix);
        button->SetBackgroundImage(MythUIButton::Selected, itemSelActPix);

        button->SetPaddingMargin(m_itemMargin);

        button->SetCheckImage(MythUIStateType::Off, checkNonePix);
        button->SetCheckImage(MythUIStateType::Half, checkHalfPix);
        button->SetCheckImage(MythUIStateType::Full, checkFullPix);

        button->SetRightArrowImage(arrowPix);

        button->SetFont(MythUIButton::Normal, *m_fontActive);
        button->SetFont(MythUIButton::Disabled, *m_fontInactive);

        button->SetPosition(GetButtonPosition(i));
        m_ButtonList.push_back(button);
    }

    itemRegPix->DownRef();
    itemSelActPix->DownRef();
    itemSelInactPix->DownRef();
    arrowPix->DownRef();
    checkNonePix->DownRef();
    checkHalfPix->DownRef();
    checkFullPix->DownRef();

    SetPositionArrowStates();
}

QPoint MythListButton::GetButtonPosition(uint i) const
{
    return QPoint(0, i * (m_itemHeight + m_itemSpacing));
}

void MythListButton::LoadPixmap(MythImage **pix, const QString& fileName)
{
    QString file = "lb-" + fileName + ".png";
    QImage *p = gContext->LoadScaleImage(file);
    *pix = MythImage::FromQImage(&p);
}

void MythListButton::LoadArrowPixmaps(MythImage **PrevReg, MythImage **PrevAct,
                                      MythImage **NextReg, MythImage **NextAct)
{
    LoadPixmap(PrevReg, "uparrow-reg");
    LoadPixmap(PrevAct, "uparrow-sel");
    LoadPixmap(NextReg, "dnarrow-reg");
    LoadPixmap(NextAct, "dnarrow-sel");
}

const QRect MythListButton::PlaceArrows(const QSize &arrowSize)
{
    int y = m_Area.height() - arrowSize.height() - 1;
    m_upArrow->SetPosition(QPoint(0, y));
    m_downArrow->SetPosition(QPoint(arrowSize.width() + m_itemMargin, y));

    return QRect(0, y, m_Area.width(), arrowSize.height());
}

QRect MythListButton::CalculateContentsRect(const QRect &arrowsRect) const
{
    return QRect(0, 0, GetArea().width(), GetArea().height() -
                 arrowsRect.height() - 2 * m_itemMargin);
}

void MythListButton::CalculateVisibleItems(void)
{
    int y = 0;
    m_itemsVisible = 0;
    while (y <= m_contentsRect.height() - m_itemHeight) 
    {
        y += m_itemHeight + m_itemSpacing;
        m_itemsVisible++;
    }
}

//////////////////////////////////////////////////////////////////////////////

MythHorizListButton::MythHorizListButton(MythUIType *parent,
                                         const char *name, const QRect &area,
                                         bool showArrow, bool showScrollArrows,
                                         uint horizontalItems)
    : MythListButton(parent, name, area, showArrow, showScrollArrows)
{
    m_itemsVisible = horizontalItems;
}

const QRect MythHorizListButton::PlaceArrows(const QSize &arrowSize)
{
    int x = GetArea().width() - arrowSize.width() - 1;
    int ytop = GetArea().height() / 2 - m_itemMargin / 2 - arrowSize.height();
    int ybottom = GetArea().height() / 2 + m_itemMargin / 2;

    m_upArrow->SetPosition(QPoint(x, ybottom));
    m_downArrow->SetPosition(QPoint(x, ytop));

    /* rightmost rectangle (full height) */
    QRect arrowsRect(x, 0, arrowSize.width() + 1, m_Area.height());

    /* now calculate the item width */
    int dx = GetArea().width() - arrowSize.width() - 1 - 2 * m_itemMargin -
             (m_itemsVisible - 1) * m_itemSpacing;
 
    /* set the item width */
    m_itemWidth = dx / m_itemsVisible;

    return QRect(x, 0, arrowSize.width() + 1, m_Area.height());
}

QRect MythHorizListButton::CalculateContentsRect(const QRect &arrowsRect) const
{
    return QRect(0, 0,
                 GetArea().width() - arrowsRect.width() - 2 * m_itemMargin,
                 GetArea().height());
}

QPoint MythHorizListButton::GetButtonPosition(uint i) const
{
    return QPoint(i * (ItemWidth() + m_itemSpacing),
                  GetArea().height() / 2 - m_itemHeight / 2);
}

void MythHorizListButton::LoadArrowPixmaps(MythImage **PrevReg,
                                           MythImage **PrevAct,
                                           MythImage **NextReg,
                                           MythImage **NextAct)
{
    LoadPixmap(PrevReg, "ltarrow-reg");
    LoadPixmap(PrevAct, "ltarrow-sel");
    LoadPixmap(NextReg, "rtarrow-reg");
    LoadPixmap(NextAct, "rtarrow-sel");
}

//////////////////////////////////////////////////////////////////////////////

MythListButtonItem::MythListButtonItem(MythListButton* lbtype, 
                                       const QString& text,
                                       MythImage *image, bool checkable,
                                       CheckState state, bool showArrow)
{
    assert(lbtype);
    
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

MythListButtonItem::~MythListButtonItem()
{
    if (m_parent)
        m_parent->RemoveItem(this);
}

QString MythListButtonItem::text() const
{
    return m_text;
}

void MythListButtonItem::setText(const QString &text)
{
    m_text = text;
}

const MythImage* MythListButtonItem::image() const
{
    return m_image;
}

void MythListButtonItem::setImage(MythImage *image)
{
    m_image = image;
}

bool MythListButtonItem::checkable() const
{
    return m_checkable;
}

MythListButtonItem::CheckState MythListButtonItem::state() const
{
    return m_state;
}

MythListButton* MythListButtonItem::parent() const
{
    return m_parent;
}

void MythListButtonItem::setChecked(MythListButtonItem::CheckState state)
{
    if (!m_checkable)
        return;
    m_state = state;
}

void MythListButtonItem::setCheckable(bool flag)
{
    m_checkable = flag;
}

void MythListButtonItem::setDrawArrow(bool flag)
{
    m_showArrow = flag;
}

void MythListButtonItem::setData(void *data)
{
    m_data = data;    
}

void *MythListButtonItem::getData()
{
    return m_data;
}

void MythListButtonItem::setOverrideInactive(bool flag)
{
    m_overrideInactive = flag;
}

bool MythListButtonItem::getOverrideInactive(void)
{
    return m_overrideInactive;
}

bool MythListButtonItem::moveUpDown(bool flag)
{
    return m_parent->MoveItemUpDown(this, flag);
}

void MythListButtonItem::SetToRealButton(MythUIButton *button, bool active_on)
{
    button->SetText(m_text, m_parent->m_textFlags);
    button->SetButtonImage(m_image);

    if (m_state == NotChecked)
        button->SetCheckState(MythUIStateType::Off);
    else if (m_state == HalfChecked)
        button->SetCheckState(MythUIStateType::Half);
    else
        button->SetCheckState(MythUIStateType::Full);

    button->EnableCheck(m_checkable);

    if (this == m_parent->m_selItem)
    {
        if (m_parent->m_active && !m_overrideInactive && active_on)
            button->SelectState(MythUIButton::Selected);
        else
        {
            if (active_on)
                button->SelectState(MythUIButton::SelectedInactive);
            else
                button->SelectState(MythUIButton::Active);
        }

        button->EnableRightArrow(m_parent->m_showArrow || m_showArrow);
    }
    else
    {
        button->SelectState(MythUIButton::Normal);
        button->EnableRightArrow(false);
    }
}

