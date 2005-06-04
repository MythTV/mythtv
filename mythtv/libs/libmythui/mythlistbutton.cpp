#include <qapplication.h>
#include <qpainter.h>
#include <qpixmap.h>

#include <cassert>

#include "mythcontext.h"
#include "mythlistbutton.h"
#include "mythmainwindow.h"
#include "mythcontext.h"
#include "mythfontproperties.h"
#include "mythuistateimage.h"

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

    m_textFlags = Qt::AlignLeft | Qt::AlignVCenter;

    SetItemRegColor(QColor("#505050"), QColor("#000000"), 100);
    SetItemSelColor(QColor("#52CA38"), QColor("#349838"), 255);

    m_itemRegPix = NULL;
    m_itemSelActPix = NULL;
    m_itemSelInactPix = NULL;
    m_arrowPix = NULL;
    m_checkNonePix = NULL;
    m_checkHalfPix = NULL;
    m_checkFullPix = NULL;
    
    m_fontActive = new MythFontProperties();
    m_fontInactive = new MythFontProperties();
}

MythListButton::~MythListButton()
{    
    Reset();
    delete m_topIterator;
    delete m_selIterator;

    if (m_itemRegPix)
        m_itemRegPix->DownRef();
    if (m_itemSelActPix)
        m_itemSelActPix->DownRef();
    if (m_itemSelInactPix)
        m_itemSelInactPix->DownRef();
    if (m_arrowPix)
        m_arrowPix->DownRef();
    if (m_checkNonePix)
        m_checkNonePix->DownRef();
    if (m_checkHalfPix)
        m_checkHalfPix->DownRef();
    if (m_checkFullPix)
        m_checkFullPix->DownRef();
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

    SetRedraw();
}

void MythListButton::SetPositionArrowStates(void)
{
    if (!m_showScrollArrows)
        return;

    if (m_itemCount == 0)
    {
        m_downArrow->DisplayImage(MythUIStateImage::Off);
        m_upArrow->DisplayImage(MythUIStateImage::Off);
    }
    else
    {
        if (m_topItem != m_itemList.first())
            m_upArrow->DisplayImage(MythUIStateImage::Full);
        else
            m_upArrow->DisplayImage(MythUIStateImage::Off);
    
        if (m_topPosition + (int)m_itemsVisible < m_itemCount)
            m_downArrow->DisplayImage(MythUIStateImage::Full);
        else
            m_downArrow->DisplayImage(MythUIStateImage::Off);
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

    SetRedraw();
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

    SetRedraw();
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

    SetRedraw();
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

    SetRedraw();
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

    SetRedraw(); 
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

    SetRedraw();
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

void MythListButton::DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                              int alphaMod)
{
    DrawSelf(p, xoffset, yoffset, true, alphaMod);
}

void MythListButton::DrawSelf(MythPainter *p, int xoffset, int yoffset, 
                              bool active_on, int alphaMod)
{
    if (!m_initialized)
        Init();

    int alpha = CalcAlpha(alphaMod);

    MythFontProperties *font = (m_active) ? m_fontActive : m_fontInactive;
    
    if (!active_on)
        font = m_fontInactive;
    
    int x = m_Area.x() + xoffset;
    int y = m_Area.y() + yoffset;
    int basey = y;

    if (m_drawFromBottom)
    {
        if (m_itemCount < (int)m_itemsVisible)
        {
            y += (m_itemsVisible - m_itemCount) * 
                 (m_itemHeight + m_itemSpacing);
        }
    }

    QPtrListIterator<MythListButtonItem> it = (*m_topIterator);
    while (it.current() && 
           (y - basey) <= (m_contentsRect.height() - m_itemHeight)) 
    {
        if (active_on && it.current()->getOverrideInactive())
        {
            font = m_fontInactive;
            it.current()->paint(p, font, x, y, active_on, alpha);
            font = (m_active) ? m_fontActive : m_fontInactive;;
        }
        else
            it.current()->paint(p, font, x, y, active_on, alpha);

        y += m_itemHeight + m_itemSpacing;

        ++it;
    }
}

void MythListButton::Init()
{
    if (m_initialized)
        return;

    m_initialized = true;

    MythPainter *painter = GetMythPainter();

    QFontMetrics fm(m_fontActive->face);
    QSize sz1 = fm.size(Qt::SingleLine, "XXXXX");
    fm = QFontMetrics(m_fontInactive->face);
    QSize sz2 = fm.size(Qt::SingleLine, "XXXXX");
    m_itemHeight = QMAX(sz1.height(), sz2.height()) + (int)(2 * m_itemMargin);

    QRect arrowsRect;
    if (m_showScrollArrows) 
    {
        m_upArrow = new MythUIStateImage(this, "upscrollarrow");
        m_downArrow = new MythUIStateImage(this, "upscrollarrow");

        MythImage *upReg, *upAct, *downReg, *downAct;
        LoadPixmap(&upReg, "uparrow-reg");
        LoadPixmap(&upAct, "uparrow-sel");
        LoadPixmap(&downReg, "dnarrow-reg");
        LoadPixmap(&downAct, "dnarrow-sel");

        m_upArrow->AddImage(MythUIStateImage::Off, upReg);
        m_upArrow->AddImage(MythUIStateImage::Full, upAct);
        m_downArrow->AddImage(MythUIStateImage::Off, downReg);
        m_downArrow->AddImage(MythUIStateImage::Full, downAct);

        m_upArrow->SetPosition(QPoint(0, 
                                      m_Area.height() - upAct->height() - 1));
        m_downArrow->SetPosition(QPoint(upAct->width() + m_itemMargin,
                                        m_Area.height() - upAct->height() - 1));

        arrowsRect = QRect(0, m_Area.height() - upAct->height() - 1,
                           m_Area.width(), upAct->height());

        upAct->DownRef();
        upReg->DownRef();
        downReg->DownRef();
        downAct->DownRef();
    }
    else 
        arrowsRect = QRect(0, 0, 0, 0);
        
    m_contentsRect = QRect(0, 0, m_Area.width(), m_Area.height() -
                           arrowsRect.height() - 2 * m_itemMargin);

    m_itemsVisible = 0;
    int y = 0;

    while (y <= m_contentsRect.height() - m_itemHeight) 
    {
        y += m_itemHeight + m_itemSpacing;
        m_itemsVisible++;
    }

    LoadPixmap(&m_checkNonePix, "check-empty");
    LoadPixmap(&m_checkHalfPix, "check-half");
    LoadPixmap(&m_checkFullPix, "check-full");
    
    LoadPixmap(&m_arrowPix, "arrow");

    QImage img(m_Area.width(), m_itemHeight, 32);
    img.setAlphaBuffer(true);

    for (int y = 0; y < img.height(); y++)
    {
        for (int x = 0; x < img.width(); x++)
        {
            uint *p = (uint *)img.scanLine(y) + x;
            *p = qRgba(0, 0, 0, m_itemRegAlpha);
        }
    }

    qApp->lock();

    {
        float rstep = float(m_itemRegEnd.red() - m_itemRegBeg.red()) / 
                      float(m_itemHeight);
        float gstep = float(m_itemRegEnd.green() - m_itemRegBeg.green()) / 
                      float(m_itemHeight);
        float bstep = float(m_itemRegEnd.blue() - m_itemRegBeg.blue()) / 
                      float(m_itemHeight);

        QPixmap itemRegPix = QPixmap(img);
        QPainter p(&itemRegPix);

        float r = m_itemRegBeg.red();
        float g = m_itemRegBeg.green();
        float b = m_itemRegBeg.blue();
        for (int y = 0; y < img.height(); y++) 
        {
            QColor c((int)r, (int)g, (int)b);
            p.setPen(c);
            p.drawLine(0, y, img.width(), y);
            r += rstep;
            g += gstep;
            b += bstep;
        }
        p.setPen(black);
        p.drawLine(0, 0, 0, img.height() - 1);
        p.drawLine(0, 0, img.width() - 1, 0);
        p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
        p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
        p.end();

        m_itemRegPix = painter->GetFormatImage();
        m_itemRegPix->Assign(itemRegPix.convertToImage());
    }   

    {
        float rstep = float(m_itemSelEnd.red() - m_itemSelBeg.red()) /
                      float(m_itemHeight);
        float gstep = float(m_itemSelEnd.green() - m_itemSelBeg.green()) / 
                      float(m_itemHeight);
        float bstep = float(m_itemSelEnd.blue() - m_itemSelBeg.blue()) /
                      float(m_itemHeight);

        QPixmap itemSelInactPix = QPixmap(img);
        QPainter p(&itemSelInactPix);

        float r = m_itemSelBeg.red();
        float g = m_itemSelBeg.green();
        float b = m_itemSelBeg.blue();
        for (int y = 0; y < img.height(); y++) 
        {
            QColor c((int)r, (int)g, (int)b);
            p.setPen(c);
            p.drawLine(0, y, img.width(), y);
            r += rstep;
            g += gstep;
            b += bstep;
        }
        p.setPen(black);
        p.drawLine(0, 0, 0, img.height() - 1);
        p.drawLine(0, 0, img.width() - 1, 0);
        p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
        p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
        p.end();

        m_itemSelInactPix = painter->GetFormatImage();
        m_itemSelInactPix->Assign(itemSelInactPix.convertToImage());

        for (int y = 0; y < img.height(); y++)
        {
            for (int x = 0; x < img.width(); x++)
            {
                uint *p = (uint *)img.scanLine(y) + x;
                *p = qRgba(0, 0, 0, 255);
            }
        }
        
        QPixmap itemSelActPix = QPixmap(img);
        p.begin(&itemSelActPix);

        r = m_itemSelBeg.red();
        g = m_itemSelBeg.green();
        b = m_itemSelBeg.blue();
        for (int y = 0; y < img.height(); y++) 
        {
            QColor c((int)r, (int)g, (int)b);
            p.setPen(c);
            p.drawLine(0, y, img.width(), y);
            r += rstep;
            g += gstep;
            b += bstep;
        }
        p.setPen(black);
        p.drawLine(0, 0, 0, img.height() - 1);
        p.drawLine(0, 0, img.width() - 1, 0);
        p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
        p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
        p.end();

        m_itemSelActPix = painter->GetFormatImage();
        m_itemSelActPix->Assign(itemSelActPix.convertToImage());
    }

    qApp->unlock();

    SetPositionArrowStates();
}

void MythListButton::LoadPixmap(MythImage **pix, const QString& fileName)
{
    QString file = "lb-" + fileName + ".png";
    QImage *p = gContext->LoadScaleImage(file);
    *pix = MythImage::FromQImage(&p);
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

    CalcDimensions();

    m_parent->InsertItem(this);
}

void MythListButtonItem::CalcDimensions(void)
{
    if (!m_parent->m_initialized)
        m_parent->Init();

    int  margin    = m_parent->m_itemMargin;
    int  width     = m_parent->m_Area.width();
    int  height    = m_parent->m_itemHeight;
    bool arrow = false;

    if (m_parent->m_showArrow || m_showArrow)
        arrow = true;

    MythImage *checkPix = m_parent->m_checkNonePix;
    MythImage *arrowPix = m_parent->m_arrowPix;
    
    int cw = checkPix->width();
    int ch = checkPix->height();
    int aw = arrowPix->width();
    int ah = arrowPix->height();
    int pw = m_image ? m_image->width() : 0;
    int ph = m_image ? m_image->height() : 0;
   
    if (m_checkable) 
        m_checkRect = QRect(margin, (height - ch)/2, cw, ch);
    else
        m_checkRect = QRect(0,0,0,0);

    if (arrow) 
        m_arrowRect = QRect(width - aw - margin, (height - ah)/2,
                            aw, ah);
    else
        m_arrowRect = QRect(0,0,0,0);

    if (m_image) 
        m_imageRect = QRect(m_checkable ? (2*margin + m_checkRect.width()) :
                            margin, (height - ph)/2,
                            pw, ph);
    else
        m_imageRect = QRect(0,0,0,0);

    m_textRect = QRect(margin +
                       (m_checkable ? m_checkRect.width() + margin : 0) +
                       (m_image    ? m_imageRect.width() + margin : 0),
                       0,
                       width - 2*margin -
                       (m_checkable ? m_checkRect.width() + margin : 0) -
                       (arrow ? m_arrowRect.width() + margin : 0) -
                       (m_image ? m_imageRect.width() + margin : 0),
                       height);
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
    CalcDimensions();
}

const MythImage* MythListButtonItem::image() const
{
    return m_image;
}

void MythListButtonItem::setImage(MythImage *image)
{
    m_image = image;
    CalcDimensions();
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
    CalcDimensions();
}

void MythListButtonItem::setDrawArrow(bool flag)
{
    m_showArrow = flag;
    CalcDimensions();
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

void MythListButtonItem::paint(MythPainter *p, MythFontProperties *font, 
                               int x, int y, bool active_on, int alpha)
{
    if (this == m_parent->m_selItem)
    {
        if (m_parent->m_active && !m_overrideInactive && active_on)
            p->DrawImage(x, y, m_parent->m_itemSelActPix, alpha);
        else
        {
            if (active_on)
                p->DrawImage(x, y, m_parent->m_itemSelInactPix, alpha);
            else
                p->DrawImage(x, y, m_parent->m_itemRegPix, alpha);
        }

        if (m_parent->m_showArrow || m_showArrow)
        {
            QRect ar(m_arrowRect);
            ar.moveBy(x, y);
            p->DrawImage(ar.x(), ar.y(), m_parent->m_arrowPix, alpha);
        }
    }
    else
    {
        p->DrawImage(x, y, m_parent->m_itemRegPix, alpha);
    }

    if (m_checkable)
    {
        QRect cr(m_checkRect);
        cr.moveBy(x, y);
        
        if (m_state == HalfChecked)
            p->DrawImage(cr.x(), cr.y(), m_parent->m_checkHalfPix, alpha);
        else if (m_state == FullChecked)
            p->DrawImage(cr.x(), cr.y(), m_parent->m_checkFullPix, alpha);
        else
            p->DrawImage(cr.x(), cr.y(), m_parent->m_checkNonePix, alpha);
    }

    if (m_image)
    {
        QRect pr(m_imageRect);
        pr.moveBy(x, y);
        p->DrawImage(pr.x(), pr.y(), m_image, alpha);
    }

    QRect tr(m_textRect);
    tr.moveBy(x, y);
    QString text = m_parent->cutDown(m_text, &(font->face), false,
                                     tr.width(), tr.height());
    p->DrawText(tr, text, m_parent->m_textFlags, *font, alpha);    
}

