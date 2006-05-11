#include <qimage.h>
#include <qmap.h>
#include <qregexp.h>

#include <iostream>
#include <algorithm>
using namespace std;

#include "yuv2rgb.h"
#include "osdtypes.h"
#include "ttfont.h"
#include "osdsurface.h"
#include "osdlistbtntype.h"
#include "osdtypeteletext.h"

#include "mythcontext.h"
#include "mythdialogs.h"

/// Shared OSD image cache
OSDImageCache OSDTypeImage::c_cache;

OSDSet::OSDSet(const QString &name, bool cache, int screenwidth, 
               int screenheight, float wmult, float hmult, int frint)
      : QObject()
{
    m_wantsupdates = false;
    m_lastupdate = 0;
    m_needsupdate = false;

    m_name = name;
    m_cache = cache;

    m_frameint = frint;

    m_hasdisplayed = false;
    m_displaying = false;
    m_timeleft = 0;
    m_allowfade = true;

    m_draweveryframe = false;

    m_screenwidth = screenwidth;
    m_screenheight = screenheight;
    m_wmult = wmult;
    m_hmult = hmult;
    m_xoffsetbase = 0;
    m_yoffsetbase = 0;

    m_notimeout = false;
    m_fadetime = -1;
    m_maxfade = -1;
 
    m_xmove = 0;
    m_ymove = 0;

    m_xoff = 0;
    m_yoff = 0;

    m_priority = 5;
    currentOSDFunctionalType = 0;

    m_showwith = QRegExp(".*");

    allTypes = new vector<OSDType *>;
}

OSDSet::OSDSet(const OSDSet &other)
      : QObject()
{
    m_screenwidth = other.m_screenwidth;
    m_screenheight = other.m_screenheight;
    m_frameint = other.m_frameint;
    m_wmult = other.m_wmult;
    m_hmult = other.m_hmult;
    m_xoffsetbase = other.m_xoffsetbase;
    m_yoffsetbase = other.m_yoffsetbase;
    m_cache = other.m_cache;
    m_name = other.m_name;
    m_notimeout = other.m_notimeout;
    m_hasdisplayed = other.m_hasdisplayed;
    m_timeleft = other.m_timeleft;
    m_displaying = other.m_displaying;
    m_fadetime = other.m_fadetime;
    m_maxfade = other.m_maxfade;
    m_priority = other.m_priority;
    m_xmove = other.m_xmove;
    m_ymove = other.m_ymove;
    m_xoff = other.m_xoff;
    m_yoff = other.m_yoff;
    m_allowfade = other.m_allowfade;
    m_wantsupdates = other.m_wantsupdates;
    m_lastupdate = other.m_lastupdate;
    m_needsupdate = other.m_needsupdate;
    m_draweveryframe = other.m_draweveryframe;

    currentOSDFunctionalType = 0;

    allTypes = new vector<OSDType *>;

    vector<OSDType *>::iterator iter = other.allTypes->begin();
    for (;iter != other.allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText*>(type))
        {
            OSDTypeText *newtext = new OSDTypeText(*item);
            AddType(newtext);
        }
        else if (OSDTypeTeletext *item = dynamic_cast<OSDTypeTeletext*>(type))
        {
            OSDTypeTeletext *newtt = new OSDTypeTeletext(*item);
            AddType(newtt);
        }
        else if (OSDTypePositionImage *item =
                  dynamic_cast<OSDTypePositionImage*>(type))
        {
            OSDTypePositionImage *newrect = new OSDTypePositionImage(*item);
            AddType(newrect);
        }
        else if (OSDTypeImage *item = dynamic_cast<OSDTypeImage*>(type))
        {
            OSDTypeImage *newimage = new OSDTypeImage(*item);
            AddType(newimage);
        }
        else if (OSDTypeBox *item = dynamic_cast<OSDTypeBox*>(type))
        {
            OSDTypeBox *newbox = new OSDTypeBox(*item);
            AddType(newbox);
        }
        else if (OSDTypePositionRectangle *item = 
                  dynamic_cast<OSDTypePositionRectangle*>(type))
        {
            OSDTypePositionRectangle *newrect = 
                               new OSDTypePositionRectangle(*item);
            AddType(newrect);
        }
        else
        {
            cerr << "Unknown conversion\n";
        }
    }
}

OSDSet::~OSDSet()
{
    Clear();
    delete allTypes;
}

void OSDSet::Clear()
{
    vector<OSDType *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        OSDType *type = (*i);
        if (type)
            delete type;
    }
    allTypes->clear();
}
 
void OSDSet::AddType(OSDType *type)
{
    typeList[type->Name()] = type;
    allTypes->push_back(type);
    type->SetParent(this);
}

void OSDSet::Reinit(int screenwidth, int screenheight, int xoff, int yoff,
                    int displaywidth, int displayheight, 
                    float wmult, float hmult, int frint)
{
    m_frameint = frint;

    m_screenwidth = screenwidth;
    m_screenheight = screenheight;
    m_wmult = wmult;
    m_hmult = hmult;
    m_xoffsetbase = xoff;
    m_yoffsetbase = yoff;

    vector<OSDType *>::iterator iter = allTypes->begin();
    for (;iter != allTypes->end(); iter++)
    {
        if (OSDTypeCC *cc608 = dynamic_cast<OSDTypeCC*>(*iter))
            cc608->Reinit(xoff, yoff, displaywidth, displayheight,
                          wmult, hmult);
        else if (OSDType708CC *cc708 = dynamic_cast<OSDType708CC*>(*iter))
            cc708->Reinit(xoff, yoff, displaywidth, displayheight);
        else
            (*iter)->Reinit(wmult, hmult);
    }
}

const OSDType *OSDSet::GetType(const QString &name) const
{
    QMap<QString,OSDType*>::const_iterator it = typeList.find(name);
    if (it != typeList.end())
        return *it;
    return NULL;
}

OSDType *OSDSet::GetType(const QString &name)
{
    QMap<QString,OSDType*>::iterator it = typeList.find(name);
    if (it != typeList.end())
        return *it;
    return NULL;
}

const OSDTypeText *OSDSet::GetSelected(void) const
{
    vector<OSDType*>::const_iterator it = allTypes->begin();
    for (; it != allTypes->end(); it++)
    {
        const OSDTypeText *item = dynamic_cast<const OSDTypeText*>(*it);
        if (item && item->IsEntry() && item->IsSelected())
            return item;
    }

    return NULL;
}

OSDTypeText *OSDSet::GetSelected(void)
{
    vector<OSDType*>::iterator it = allTypes->begin();
    for (; it != allTypes->end(); it++)
    {
        OSDTypeText *item = dynamic_cast<OSDTypeText*>(*it);
        if (item && item->IsEntry() && item->IsSelected())
            return item;
    }

    return NULL;
}

void OSDSet::ClearAllText(void)
{
    vector<OSDType *>::iterator iter = allTypes->begin();
    for (; iter != allTypes->end(); iter++)
    {
        OSDType *type = (*iter);
        if (OSDTypeText *item = dynamic_cast<OSDTypeText *>(type))
        {
            QString defText = item->GetDefaultText();
            if ((defText == "") ||
                (defText.contains(QRegExp("%"))))
                item->SetText(QString(""));
        }
    }
}

void OSDSet::SetText(const QMap<QString, QString> &infoMap)
{
    vector<OSDType *>::iterator it = allTypes->begin();
    for (; it != allTypes->end(); it++)
    {
        OSDTypeText *item = dynamic_cast<OSDTypeText*>(*it);
        if (!item)
            continue;

        QMap<QString, QString>::const_iterator riter = infoMap.begin();
        QString new_text = item->GetDefaultText();

        if ((new_text == "") && (infoMap.contains(item->Name())))
        {
            new_text = infoMap[item->Name()];
        }
        else if (new_text.contains(QRegExp("%.*%")))
        {
            for (; riter != infoMap.end(); riter++)
            {
                QString key  = riter.key().upper();
                QString data = riter.data();

                if (new_text.contains(key))
                {
                    QRegExp full_regex(
                        QString("%") + key +
                        QString("(\\|([^%|]*))?") +
                        QString("(\\|([^%|]*))?") +
                        QString("(\\|([^%]*))?%"));

                    if (riter.data() != "")
                    {
                        new_text.replace(full_regex,
                                         QString("\\2") + data + "\\4");
                    }
                    else
                    {
                        new_text.replace(full_regex, "\\6");
                    }
                }
            }
        }

        if (new_text != "")
            item->SetText(new_text);
    }
    m_needsupdate = true;
}

void OSDSet::GetText(QMap<QString, QString> &infoMap) const
{
    vector<OSDType*>::const_iterator it = allTypes->begin();
    for (; it != allTypes->end(); it++)
    {
        OSDTypeText *item = dynamic_cast<OSDTypeText *>(*it);
        if (item && item->IsEntry())
            infoMap[item->Name()] = item->GetText();
    }
}

bool OSDSet::SetSelected(int index)
{
    bool success = false;

    if (index < 0)
    {
        OSDTypeText *max_item = NULL;
        int max_item_num = -1;

        vector<OSDType*>::iterator it = allTypes->begin();
        for (; it != allTypes->end(); it++)
        {
            OSDTypeText *item = dynamic_cast<OSDTypeText *>(*it);
            if (item && item->IsEntry() && item->IsSelected())
                item->SetSelected(false);
            if (item && item->GetEntryNum() > max_item_num)
            {
                max_item = item;
                max_item_num = item->GetEntryNum();
            }
        }
        if (max_item)
            max_item->SetSelected(success = true);

        m_needsupdate = true;
        return success;
    }

    vector<OSDType*>::iterator it = allTypes->begin();
    for (; it != allTypes->end(); it++)
    {
        OSDTypeText *item = dynamic_cast<OSDTypeText *>(*it);
        if (item && item->IsEntry() && item->IsSelected())
            item->SetSelected(false);
        if (item && (item->GetEntryNum() == index))
            item->SetSelected(success = true);
    }

    m_needsupdate = true;
    return success;
}

bool OSDSet::HandleKey(const QKeyEvent *e, bool *focus_change,
                       QString *button_pressed)
{
    QStringList actions;
    int move = 0;
    int del  = 0;
    int field_move = 0;

    if (focus_change)
        *focus_change = false;
    if (button_pressed)
        *button_pressed = "";

    int     key = e->key();
    QString txt = e->text();
    if (key == Qt::Key_Escape)
    {
        SetSelected(0);
        Hide();
        return true;
    }
    else if (Qt::Key_Down == key || Qt::Key_Tab == key)
        field_move = +1;
    else if (Qt::Key_Up == key || Qt::Key_BackTab == key)
        field_move = -1;
    else if (Qt::Key_Right == key)
        move = +1;
    else if (Qt::Key_Left == key)
        move = -1;
    else if (Qt::Key_Delete == key)
        del = +1;
    else if (Qt::Key_BackSpace == key)
        del = -1;
    else if (e->state() == Qt::ControlButton)
    {
        if (Qt::Key_A == key)
            move = -1000;
        else if (Qt::Key_E == key)
            move = +1000;
        else if (Qt::Key_F == key)
            move = +1;
        else if (Qt::Key_B == key)
            move = -1;
        else if (Qt::Key_U == key)
            del = -1000;
        else if (Qt::Key_K == key)
            del = +1000;
        else if (Qt::Key_D == key)
            del = +1;
        else if (Qt::Key_H == key)
            del = -1;
        else
        {
            const QString btn = HandleHotKey(e);
            if (!btn.isEmpty())
            {
                if ((btn == "ok") || (btn == "cancel"))
                    Hide();

                if (button_pressed)
                    *button_pressed = btn;

                return true;
            }
        }
    }

    OSDTypeText *item = GetSelected();
    if (!item)
        return false;

    if (item->IsButton() && !field_move)
    {
        if (txt != "\n" && txt != "\r" && txt != " ")
            return false;

        const QString btn = item->Name().lower();
        if ((btn == "ok") || (btn == "cancel"))
            Hide();

        if (button_pressed)
            *button_pressed = btn;

        return true;
    }

    if (!item->IsButton() && (Qt::Key_Return == key || Qt::Key_Enter == key))
        field_move = +1;

    if (field_move != 0)
    {
        if (focus_change)
            *focus_change = true;

        int next = (int)item->GetEntryNum() + field_move;
        if (next >= 0 && SetSelected(next))
            return true;

        return SetSelected((field_move > 0) ? 0 : -1);
    }

    if (move != 0)
        item->MoveCursor(move);
    else if (del != 0)
        item->Delete(del);
    else if ((1 == txt.length()) && txt.at(0).isPrint() &&
             (txt != "[") && (txt != "]"))
    {
        item->InsertCharacter(txt.at(0));
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Unhandled event");
        return false;
    }

    m_needsupdate = true;
    return true;
}

static int extract_hot_key(const QString &text)
{
    int i = text.find("[");
    if ((i < 0) || ((i + 1) >= (int)text.length()))
        return 0;
    int ch = text.at(i + 1).upper() - 'A' + Qt::Key_A;
    if (ch >= Qt::Key_A && ch <= Qt::Key_Z)
        return ch;
    return 0;
}

QString OSDSet::HandleHotKey(const QKeyEvent *e)
{
    vector<OSDType*>::iterator it = allTypes->begin();
    for (; it != allTypes->end(); it++)
    {
        OSDTypeText *item = dynamic_cast<OSDTypeText *>(*it);
        if (item && item->IsButton() &&
            extract_hot_key(item->GetText()) == e->key())
        {
            return item->Name().lower();
        }
    }
    return QString::null;
}

void OSDSet::Display(bool onoff, int osdFunctionalType)
{
    if (onoff)
    {
        m_notimeout = true;
        m_displaying = true;
        m_timeleft = 1;
        m_fadetime = -1;
        m_xoff = 0;
        m_yoff = 0;
    }
    else
    {
        m_displaying = false;
    }

    if (currentOSDFunctionalType != osdFunctionalType && 
        currentOSDFunctionalType != 0)
    {
        emit OSDClosed(currentOSDFunctionalType);
    }

    currentOSDFunctionalType = osdFunctionalType;
}

void OSDSet::DisplayFor(int time, int osdFunctionalType)
{
    m_timeleft = time;
    m_displaying = true;
    m_fadetime = -1;
    m_notimeout = false;
    m_xoff = 0;
    m_yoff = 0;

    if (currentOSDFunctionalType != osdFunctionalType && 
        currentOSDFunctionalType != 0)
    {
        emit OSDClosed(currentOSDFunctionalType);
    }

    currentOSDFunctionalType = osdFunctionalType;
}
 
/*void OSDSet::FadeFor(int time)
{
    cerr<<"fadefor("<<time<<")"<<endl;
    if (m_allowfade)
    {
        m_timeleft = -1;
        m_fadetime = time;
        m_maxfade = time;
        m_displaying = true;
        m_notimeout = false;
    }
}
*/

void OSDSet::Hide(void)
{
    m_timeleft = -1;
    m_fadetime = 0;
    m_notimeout = false;
    m_displaying = false;

    if (currentOSDFunctionalType)
    {
        emit OSDClosed(currentOSDFunctionalType);
        currentOSDFunctionalType = 0;
    }
}

static int round_uts(int time) { return (time + 999999) / 1000000; }

void OSDSet::Draw(OSDSurface *surface, bool actuallydraw)
{
    // We need to prevent m_fade from being zeroed earlier to force
    // one more blanking draw, so we force a lower bound of one in the
    // fade time calc below, here we fix this so that the OSD can fade
    // out completely (Otherwise IA44 OSD can leave a ghost behind).
    m_fadetime = (1 == m_fadetime) ? 0 : m_fadetime;

    if (actuallydraw && m_displaying)
    {
        vector<OSDType *>::iterator i = allTypes->begin();
        for (; i != allTypes->end(); i++)
        {
            OSDType *type = (*i);
            type->Draw(surface, m_fadetime, m_maxfade, m_xoff + m_xoffsetbase,
                       m_yoff + m_yoffsetbase);

            if (m_wantsupdates)
                m_lastupdate = round_uts(m_timeleft);
        }
    }

    m_hasdisplayed |= m_displaying;

    // If we needed an update and draw was processed, we no longer need draw
    m_needsupdate  &= !actuallydraw;
    // If m_draweveryframe is set or one second has elapsed since
    // the last deaw m_wantupdates is set.
    m_needsupdate  |= m_draweveryframe ||
        (m_wantsupdates && (round_uts(m_timeleft) != m_lastupdate));

    // If OSD is still visible and we have time outs, count down
    // timeouts and do any animation and/or emit that must be done.
    if (!m_notimeout && m_displaying)
    {
        // Count down pre-fade time 
        m_timeleft = max(m_timeleft - m_frameint, 0);

        // If fading, start counting down fade time
        if (IsFading())
            m_fadetime = max(m_fadetime - m_frameint, 1);
        // Do a move animation if a fade and (m_xmove or m_ymove) exist
        if (IsFading() && (m_xmove || m_ymove))
        {
            m_xoff += (m_xmove * m_frameint * 30) / 1000000;
            m_yoff += (m_ymove * m_frameint * 30) / 1000000;
            m_fadetime = max(m_fadetime - (4 * m_frameint), 1);
        }

        // The OSD is displaying up until the two timeouts are <= zero
        m_displaying   = m_timeleft > 0 || m_fadetime > 0;

        // Make sure we repaint if the OSD is now gone
        m_needsupdate |= !m_displaying;

        // Emit any functional type if the OSD is now gone.
        if (!m_displaying && currentOSDFunctionalType)
        {
            emit OSDClosed(currentOSDFunctionalType);
            currentOSDFunctionalType = 0;
        }
    }
}


bool OSDSet::CanShowWith(const QString &name) const
{
    return m_showwith.exactMatch(name);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 

OSDType::OSDType(const QString &name)
{
    m_hidden = false;
    m_name = name;
}

OSDType::~OSDType()
{
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeText::OSDTypeText(const QString &name, TTFFont *font, 
                         const QString &text, QRect displayrect,
                         float wmult, float hmult)
           : OSDType(name)
{
    m_message = text;
    m_default_msg = text;
    m_font = font;

    m_altfont = NULL;
    
    m_displaysize = displayrect;
    m_screensize = displayrect;
    m_multiline = false;
    m_centered = false;
    m_right = false;
    m_usingalt = false;

    m_selected  = false;
    m_button    = false;
    m_entrynum  = -1;
    m_cursorpos = 0;

    m_scroller = false;
    m_scrollx = m_scrolly = 0;
    m_scrollinit = false;

    m_linespacing = 1.5;

    m_unbiasedsize = unbias(m_screensize, wmult, hmult);

    m_draw_info_str = "";
    m_draw_info_len = 0;
}

OSDTypeText::OSDTypeText(const OSDTypeText &other)
           : OSDType(other.m_name)
{
    m_message = other.m_message;
    m_default_msg = other.m_default_msg;
    m_font = other.m_font;

    m_altfont = other.m_altfont;

    m_displaysize = other.m_displaysize;
    m_screensize = other.m_screensize; 
    m_multiline = other.m_multiline;
    m_centered = other.m_centered;
    m_right = other.m_right;
    m_usingalt = other.m_usingalt; 
   
    m_selected = other.m_selected; 
    m_button   = other.m_button; 
    m_entrynum = other.m_entrynum; 
    m_cursorpos = other.m_cursorpos; 
 
    m_scroller = other.m_scroller;
    m_scrollx = other.m_scrollx;
    m_scrolly = other.m_scrolly;
    m_scrollinit = other.m_scrollinit;

    m_linespacing = other.m_linespacing;

    m_unbiasedsize = other.m_unbiasedsize;

    m_draw_info_str = "";
    m_draw_info_len = 0;
}

OSDTypeText::~OSDTypeText()
{
}

void OSDTypeText::SetAltFont(TTFFont *font)
{
    m_altfont = font;
}

void OSDTypeText::SetText(const QString &text)
{
    m_message = text;
    m_cursorpos = text.length();
    m_scrollinit = false;
}

void OSDTypeText::SetDefaultText(const QString &text)
{
    m_message = text;
    m_default_msg = text;
    m_scrollinit = false;
}

void OSDTypeText::Reinit(float wmult, float hmult)
{
    m_displaysize = m_screensize = bias(m_unbiasedsize, wmult, hmult);
}

void OSDTypeText::Draw(OSDSurface *surface, int fade, int maxfade, int xoff, 
                       int yoff)
{
    int textlength = 0;

    if (m_message == QString::null)
        return;

    if (m_message.contains("%d"))
        m_parent->SetWantsUpdates(true);

    if (m_scroller)
        m_parent->SetDrawEveryFrame(true);

    m_font->CalcWidth(m_message, &textlength);

    int maxlength = m_displaysize.width();

    if (m_multiline)
    {
        QString tmp_msg = m_message;
        tmp_msg.replace(QRegExp("%BR%"), "\n");
        tmp_msg.replace(QRegExp("\n")," \n ");

        QStringList wordlist = QStringList::split(" ", tmp_msg);
        int length = 0;
        int lines = 0;

        QString line = "";

        QStringList::iterator it = wordlist.begin();
        for (; it != wordlist.end(); ++it)
        {
            QString word = *it;
            if (word == "%d") 
            {
                m_parent->SetWantsUpdates(true);
                int timeleft = (m_parent->GetTimeLeft() + 999999) / 1000000;
                if (timeleft > 99)
                    timeleft = 99;
                if (timeleft < 0)
                    timeleft = 0;
                
                word = QString::number(timeleft);    
            }

            if (!length && word == "\n")
                continue;

            m_font->CalcWidth(word, &textlength);
            if ((textlength + m_font->SpaceWidth() + length > maxlength) ||
                (word == "\n"))
            {
                QRect drawrect = m_displaysize;
                drawrect.setTop((int)(m_displaysize.top() + m_font->Size() * 
                                      (lines) * m_linespacing));
                DrawString(surface, drawrect, line, fade, maxfade, xoff, yoff);
                length = 0;

                line = "";
                lines++;

                if (word == "\n")
                {
                    word = "";
                    textlength = 0;
                }
            }

            if (length == 0)
            {
                length = textlength;
                line = word;
            }
            else
            {
                line += " " + word;
                length += textlength + m_font->SpaceWidth();
            }
        }

        QRect drawrect = m_displaysize;
        drawrect.setTop((int)(m_displaysize.top() + m_font->Size() * (lines) *
                              m_linespacing));
        DrawString(surface, drawrect, line, fade, maxfade, xoff, yoff);
    }           
    else if (m_scroller)
    {
        if (!m_scrollinit)
        {
            m_displaysize = m_screensize;
            if (m_scrollx < 0)
            {
                int numspaces = m_displaysize.width() / m_font->SpaceWidth();
                for (int i = 0; i < numspaces; i++)
                    m_message.prepend(" ");

                int messagewidth = 0;
                m_font->CalcWidth(m_message, &messagewidth);
                m_scrollstartx = 0;
                m_scrollendx = 0 - (messagewidth);
                m_scrollposx = m_scrollstartx;
                m_displaysize.setWidth(messagewidth);
            }
            else if (m_scrollx > 0)
            {
                int messagewidth = 0;
                m_font->CalcWidth(m_message, &messagewidth);
                m_scrollstartx = 0 - (messagewidth);
                m_scrollendx = m_displaysize.width();
                m_scrollposx = m_scrollstartx;
                m_displaysize.setWidth(messagewidth);
            }
            
            m_scrollstarty = 0;
            m_scrollendy = 0;
            m_scrollposy = 0;

            m_scrollinit = true;
        }
        else
        {
            m_scrollposx += m_scrollx;
            m_scrollposy += m_scrolly;

            if ((m_scrollx < 0 && m_scrollposx < m_scrollendx) ||
                (m_scrollx > 0 && m_scrollposx > m_scrollendx))
                m_parent->Hide();
        }

        DrawString(surface, m_displaysize, m_message, fade, maxfade, 
                   xoff + m_scrollposx, yoff + m_scrollposy);
    }
    else if (!m_message.contains("["))
    {
        DrawString(surface, m_displaysize, m_message,
                   fade, maxfade, xoff, yoff);
    }
    else
    {
        DrawHiLiteString(surface, m_displaysize, m_message,
                         fade, maxfade, xoff, yoff);
    }
}

void OSDTypeText::DrawHiLiteString(OSDSurface *surface, QRect rect, 
                                   const QString &text, int fade, int maxfade, 
                                   int xoff, int yoff, bool double_size)
{
    if (m_draw_info_str != text)
    {
        m_draw_info_str = QDeepCopy<QString>(text);
        m_draw_info.clear();
        m_draw_info_len = 0;

        bool    in_hilite = false;
        QString str       = text;

        while (!str.isEmpty())
        {
            QString tmp = str;
            int loc = str.find((in_hilite) ? "]" : "[");

            if (loc < 0)
            {
                str = QString::null;
            }
            else
            {
                tmp = str.left(loc);
                str = str.mid(loc + 1);
            }

            if (!tmp.isEmpty())
            {
                int msgwidth = 0;
                TTFFont *font = (in_hilite || m_selected) ? m_altfont : m_font;
                if (font)
                    font->CalcWidth(tmp, &msgwidth);
                m_draw_info_len += msgwidth;
                m_draw_info.push_back(DrawInfo(tmp, msgwidth, in_hilite));
            }

            in_hilite = !in_hilite;
        }
    }

    if (m_centered || m_right)
    {
        int xoffset = rect.width() - m_draw_info_len;
        if (m_centered)
            xoffset /= 2;

        if (xoffset > 0)
            rect.moveBy(xoffset, 0);
    }

    rect.moveBy(xoff, yoff);
    rect.setRight( min(rect.right(),  surface->width));
    rect.setBottom(min(rect.bottom(), surface->height));

    QRect off_rect = rect;
    for (uint i = 0; i < m_draw_info.size(); i++)
    {
        int alphamod = 255;
        if (maxfade > 0 && fade >= 0)
            alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

        bool use_alt = m_draw_info[i].hilite || m_selected;
        TTFFont *font = (use_alt) ? m_altfont : m_font;
        if (font && (off_rect.width() > 0) && (off_rect.height() > 0))
        {
            font->DrawString(surface, off_rect.left(), off_rect.top(),
                             m_draw_info[i].msg,
                             off_rect.right(), off_rect.bottom(),
                             alphamod, double_size);

            off_rect.moveBy(m_draw_info[i].width, 0);
            rect.setRight( min(rect.right(),  surface->width));
            rect.setBottom(min(rect.bottom(), surface->height));
        }
    }

    // draw cursor
    if (IsSelected() && !IsButton())
    {
        xoff = 0;
        if (m_cursorpos > 0)
        {
            int i = 0, w = 0, len = 0, pos = m_cursorpos;
            for (; i < (int) m_draw_info.size(); i++)
            {
                len += m_draw_info[i].msg.length();
                if (len > m_cursorpos)
                    break;
                xoff += m_draw_info[i].width;
                pos  -= m_draw_info[i].msg.length();
            }
            QString tmp = m_draw_info[i].msg.left(max(pos,0));
            if (!tmp.isEmpty())
            {
                bool use_alt = m_draw_info[i].hilite || m_selected;
                TTFFont *font = (use_alt) ? m_altfont : m_font;
                font->CalcWidth(tmp, &w);
                xoff += w;
            }
        }

        QRect crect(rect.top(), rect.left(), 2, (m_font->Size() * 3) / 2);
        if (crect.right() < surface->width && crect.right() < rect.right())
        {
            OSDTypeBox box("cursor", crect, 1.0f, 1.0f);
            box.SetColor(Qt::white);
            box.Draw(surface, fade, maxfade, xoff, 0, 200);
        }
    }
}
  
void OSDTypeText::DrawString(OSDSurface *surface, QRect rect, 
                             const QString &text, int fade, int maxfade, 
                             int xoff, int yoff, bool doubl)
{
    if (m_centered || m_right)
    {
        int textlength = 0;
        m_font->CalcWidth(text, &textlength);

        int xoffset = rect.width() - textlength;
        if (m_centered)
            xoffset /= 2;

        if (xoffset > 0)
            rect.moveBy(xoffset, 0);
    }

    rect.moveBy(xoff, yoff);
    rect.setRight( min(rect.right(),  surface->width));
    rect.setBottom(min(rect.bottom(), surface->height));

    int alphamod = 255;
    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    TTFFont *font = m_font;
    if ((m_usingalt || m_selected) && m_altfont)
        font = m_altfont;

    font->DrawString(surface, rect.left(), rect.top(), text,
                     rect.right(), rect.bottom(), alphamod, doubl);

    // draw cursor
    if (IsSelected() && !IsButton())
    {
        xoff = 0;
        if (m_cursorpos > 0)
            m_font->CalcWidth(text.left(m_cursorpos), &xoff);

        QRect crect(rect.left(), rect.top(), 2, (m_font->Size() * 3) / 2);
        if (crect.right() < surface->width && crect.right() < rect.right())
        {
            OSDTypeBox box("cursor", crect, 1.0f, 1.0f);
            box.SetColor(Qt::white);
            box.Draw(surface, fade, maxfade, xoff, 0, 200);
        }
    }
} 

int clamp(int val, int minimum, int maximum)
{
    return min(max(val, minimum), maximum);
}

bool OSDTypeText::MoveCursor(int dir)
{
    if (!IsEntry() || IsButton())
        return false;

    int new_pos = m_cursorpos + dir;
    m_cursorpos = clamp(new_pos, 0, (int)m_message.length());
    return new_pos == m_cursorpos;
}

bool OSDTypeText::Delete(int dir)
{
    if (!IsEntry() || IsButton())
        return false;

    if (dir > 0)
    {
        m_message.remove(m_cursorpos, dir);
        return true;
    }

    if (dir < 0)
    {
        int cpos = max(m_cursorpos + dir, 0);
        m_message.remove(cpos, abs(cpos - m_cursorpos));
        m_cursorpos = cpos;
        return true;
    }

    return false;
}

void OSDTypeText::InsertCharacter(QChar ch)
{
    if (!IsEntry() || IsButton())
        return;

    m_message.insert(m_cursorpos, ch);
    MoveCursor(+1);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeImage::OSDTypeImage(const QString &name, const QString &filename, 
                           QPoint displaypos, float wmult, float hmult, 
                           int scalew, int scaleh)
            : OSDType(name)
{
    m_drawwidth = -1;
    m_onlyusefirst = false;

    m_filename = filename;
    SetPosition(displaypos, wmult, hmult);

    m_yuv = m_alpha = NULL;
    m_isvalid = false;
    m_imagesize = QRect(0, 0, 0, 0);

    m_scalew = scalew;
    m_scaleh = scaleh;
    m_dontround = false;
    m_cacheitem = NULL;

    LoadImage(filename, wmult, hmult, scalew, scaleh);
}

OSDTypeImage::OSDTypeImage(const OSDTypeImage &other)
            : OSDType(other.m_name)
{
    m_drawwidth = other.m_drawwidth;
    m_onlyusefirst = other.m_onlyusefirst;

    m_filename = other.m_filename;
    m_displaypos = other.m_displaypos;
    m_imagesize = other.m_imagesize;
    m_isvalid = other.m_isvalid;
    m_name = other.m_name;
    m_scalew = other.m_scalew;
    m_scaleh = other.m_scaleh;
    m_cacheitem = NULL;
    m_dontround = other.m_dontround;

    m_alpha = m_yuv = NULL;
    if (m_isvalid)
    {
        int size = m_imagesize.width() * m_imagesize.height() * 3 / 2;
        m_yuv = new unsigned char[size];
        memcpy(m_yuv, other.m_yuv, size);


        size = m_imagesize.width() * m_imagesize.height();
        m_alpha = new unsigned char[size];
        memcpy(m_alpha, other.m_alpha, size);

        m_ybuffer = m_yuv;
        m_ubuffer = m_yuv + (m_imagesize.width() * m_imagesize.height());
        m_vbuffer = m_yuv + (m_imagesize.width() * m_imagesize.height() * 
                             5 / 4);
    } 
}

OSDTypeImage::OSDTypeImage(const QString &name)
            : OSDType(name)
{
    m_drawwidth = -1;
    m_onlyusefirst = false;

    m_displaypos = QPoint(0, 0);
    m_unbiasedpos = QPoint(0, 0);

    m_yuv = NULL;
    m_alpha = NULL;
    m_ybuffer = NULL;
    m_ubuffer = NULL;
    m_vbuffer = NULL;
    m_isvalid = false;
    m_filename = "";
    m_cacheitem = NULL;
    m_dontround = false;
}

OSDTypeImage::OSDTypeImage(void)
            : OSDType("")
{
    m_name = "";
    m_drawwidth = -1;
    m_onlyusefirst = false;

    m_displaypos = QPoint(0, 0);
    m_unbiasedpos = QPoint(0, 0);

    m_yuv = NULL;
    m_alpha = NULL;
    m_ybuffer = NULL;
    m_ubuffer = NULL;
    m_vbuffer = NULL;
    m_isvalid = false;
    m_filename = "";
    m_cacheitem = NULL;
    m_dontround = false;
}

OSDTypeImage::~OSDTypeImage()
{
    // In case we have a cache item in hand, it's safe to delete it,
    // as it should not be in OSDImageCache anymore and it should have
    // been written to the file cache for faster access in the future.
    delete m_cacheitem;
    m_cacheitem = NULL;    
}

void OSDTypeImage::SetName(const QString &name)
{
    m_name = name;
}

void OSDTypeImage::SetPosition(QPoint pos, float wmult, float hmult)
{
    m_displaypos  = pos;
    m_unbiasedpos =
        QPoint((int)round(pos.x() / wmult),
               (int)round(pos.y() / hmult));
}

void OSDTypeImage::Reinit(float wmult, float hmult)
{
    m_displaypos =
        QPoint((int)round(m_unbiasedpos.x() * wmult),
               (int)round(m_unbiasedpos.y() * hmult));

    LoadImage(m_filename, wmult, hmult, m_scalew, m_scaleh);
}

void OSDTypeImage::LoadImage(const QString &filename, float wmult, float hmult,
                             int scalew, int scaleh)
{
    QString ckey;
   
    if (!filename.isEmpty() && filename.length() >= 2)
    {
        ckey = OSDImageCache::CreateKey(
            filename, wmult, hmult, scalew, scaleh);
    }
    else 
    {
        // this method requires a backing file
        return;
    }
  
    // Get the item from the cache so it's not freed while in use
    OSDImageCacheValue* value = c_cache.Get(ckey, true);
  
    if (value != NULL)
    {
        m_yuv       = value->m_yuv;
        m_ybuffer   = value->m_ybuffer;
        m_ubuffer   = value->m_ubuffer;
        m_vbuffer   = value->m_vbuffer;
        m_alpha     = value->m_alpha;
        m_imagesize = value->m_imagesize;
        m_isvalid   = true;

        // Put the old image back to the cache so it can be reused in the
        // future, and possibly freed by the cache system if the size limit
        // is reached
        if (!m_cacheitem)
            c_cache.Insert(m_cacheitem);
        m_cacheitem = value;

        return;
    }
   
    // scaled image was not found in cache, have to create it
  

    QImage tmpimage(filename);

    if (tmpimage.width() == 0)
        return;

    if (scalew > 0 && m_scalew > 0)
        scalew = m_scalew;
    if (scaleh > 0 && m_scaleh > 0)
        scaleh = m_scaleh;

    int imwidth = 0, imheight = 0;

    if (scalew > 0) 
        imwidth = ((int)(scalew * wmult) / 2) * 2;
    else
        imwidth = ((int)(tmpimage.width() * wmult) / 2) * 2;

    if (scaleh > 0)
        imheight = ((int)(scaleh * hmult) / 2) * 2;
    else
        imheight = ((int)(tmpimage.height() * hmult) / 2) * 2;

    QImage tmp2 = tmpimage.smoothScale(imwidth, imheight);

    m_isvalid = true;

    m_yuv = new unsigned char[imwidth * imheight * 3 / 2];
    m_ybuffer = m_yuv;
    m_ubuffer = m_yuv + (imwidth * imheight);
    m_vbuffer = m_yuv + (imwidth * imheight * 5 / 4);

    m_alpha = new unsigned char[imwidth * imheight];

    rgb32_to_yuv420p(m_ybuffer, m_ubuffer, m_vbuffer, m_alpha, tmp2.bits(),
                     imwidth, imheight, tmp2.width());

    m_imagesize = QRect(0, 0, imwidth, imheight);

    // put the old image back to the cache so it can be reused in the
    // future, and possibly freed by the cache system if the size limit
    // is reached
    if (m_cacheitem)
        c_cache.Insert(m_cacheitem);
  
    m_cacheitem = new OSDImageCacheValue(
        ckey,
        m_yuv,     m_ybuffer, m_ubuffer,
        m_vbuffer, m_alpha,   m_imagesize);
 
    // save the new cache item to the file cache
    if (!filename.isEmpty())
        c_cache.SaveToDisk(m_cacheitem);
}

void OSDTypeImage::LoadFromQImage(const QImage &img)
{
    // this method is not cached as it's used mostly for
    // subtitles which are displayed only once anyways, caching
    // would probably only slow things down overall
    if (m_isvalid)
    {
        delete m_cacheitem;
        m_cacheitem = NULL;
        m_isvalid = false;
        m_yuv = NULL;
        m_alpha = NULL;
    }

    m_isvalid = true;

    int imwidth = (img.width() + 1) & ~1;
    int imheight = (img.height() + 1) & ~1;

    m_yuv = new unsigned char[imwidth * imheight * 3 / 2];
    m_ybuffer = m_yuv;
    m_ubuffer = m_yuv + (imwidth * imheight);
    m_vbuffer = m_yuv + (imwidth * imheight * 5 / 4);

    m_alpha = new unsigned char[imwidth * imheight];

    rgb32_to_yuv420p(m_ybuffer, m_ubuffer, m_vbuffer, m_alpha, img.bits(),
                     img.width(), img.height(), img.bytesPerLine() / 4);

    m_imagesize = QRect(0, 0, imwidth, imheight);
}

void OSDTypeImage::Draw(OSDSurface *surface, int fade, int maxfade,
                        int xoff, int yoff)
{
    if (m_hidden)
        return;

    if (!m_isvalid)
        return;

    int xstart = 0, ystart = 0, startcol = 0, startline = 0;

    if (m_dontround)
    {
        xstart = m_displaypos.x() + xoff;
        ystart = m_displaypos.y() + yoff;
    }
    else
    {
        xstart = ((m_displaypos.x() + xoff + 1) / 2) * 2;
        ystart = ((m_displaypos.y() + yoff)     / 2) * 2;
    }

    if (ystart < 0)
    {
        startline = 0 - ystart;
        ystart = 0;
    }

    if (xstart < 0)
    {
        startcol = 0 - xstart;
        xstart = 0;
    }

    int iwidth     = m_imagesize.width();
    int drawwidth  = (m_drawwidth >= 0) ? m_drawwidth : m_imagesize.width();
    int drawheight = m_imagesize.height();

    drawwidth = (drawwidth + xstart > surface->width) ?
        surface->width - xstart - 1 : drawwidth;

    drawheight = (drawheight + ystart > surface->height) ?
        surface->height - ystart - 1 : drawheight;
 
    if ((drawwidth <= 0) || (drawheight <= 0))
        return;

    QRect destRect = QRect(xstart, ystart, drawwidth, drawheight);
    bool needblend = m_onlyusefirst || surface->IntersectsDrawn(destRect);

    surface->AddRect(destRect);
    
    int alphamod = 255;

    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    if (!needblend)
    {
        for (int y = startline; y < drawheight; y++)
        {
            int ysrcwidth = y * iwidth;
            int ydestwidth = (y + ystart - startline) * surface->width;

            memcpy(surface->y + xstart + ydestwidth,
                   m_ybuffer + startcol + ysrcwidth, drawwidth);

            unsigned char *destalpha = surface->alpha + xstart + ydestwidth;

            for (int x = startcol; x < drawwidth; x++)
            {
                int alpha = *(m_alpha + x + ysrcwidth);
  
                if (alpha == 0)
                    *destalpha = 0;
                else
                    *destalpha = ((alpha * alphamod) + 0x80) >> 8;

                destalpha++;
            }
        }

        iwidth     /= 2;
        drawwidth  /= 2;
        drawheight /= 2;
        ystart     /= 2;
        xstart     /= 2;
        startline  /= 2;
        startcol   /= 2;

        for (int y = startline; y < drawheight; y++)
        {
            int uvsrcwidth = y * iwidth;
            int uvdestwidth = (y + ystart - startline) * (surface->width / 2);

            memcpy(surface->u + xstart + uvdestwidth,
                   m_ubuffer + startcol + uvsrcwidth, drawwidth);
            memcpy(surface->v + xstart + uvdestwidth,
                   m_vbuffer + startcol + uvsrcwidth, drawwidth);
        }

        return;
    }

    int ysrcwidth   = startline * iwidth;
    int uvsrcwidth  = ysrcwidth  / 4;
    int startingx   = (m_onlyusefirst) ? 0 : startcol;

    unsigned char *src       = m_ybuffer      + ysrcwidth  + startingx;
    unsigned char *usrc      = m_ubuffer      + uvsrcwidth + startingx / 2;
    unsigned char *vsrc      = m_vbuffer      + uvsrcwidth + startingx / 2;
    unsigned char *srcalpha  = m_alpha        + ysrcwidth  + startingx;

    int ydestwidth  = ystart * surface->width;
    int uvdestwidth = ydestwidth / 4;

    unsigned char *dest      = surface->y     + xstart     + ydestwidth;
    unsigned char *udest     = surface->u     + xstart / 2 + uvdestwidth;
    unsigned char *vdest     = surface->v     + xstart / 2 + uvdestwidth;
    unsigned char *destalpha = surface->alpha + xstart     + ydestwidth;

    if (m_onlyusefirst)
        (surface->blendcolumnfunc) (src, usrc, vsrc, srcalpha, iwidth, dest,
                                    udest, vdest, destalpha, surface->width,
                                    drawwidth - startcol, 
                                    drawheight - startline,
                                    alphamod, 1, surface->rec_lut,
                                    surface->pow_lut);
    else
        (surface->blendregionfunc) (src, usrc, vsrc, srcalpha, iwidth, dest,
                                    udest, vdest, destalpha, surface->width,
                                    drawwidth - startcol, 
                                    drawheight - startline,
                                    alphamod, 1, surface->rec_lut,
                                    surface->pow_lut);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypePosSlider::OSDTypePosSlider(const QString &name,
                                   const QString &filename,
                                   QRect displayrect, float wmult,
                                   float hmult, int scalew, int scaleh)
               : OSDTypeImage(name, filename, displayrect.topLeft(), wmult,
                              hmult, scalew, scaleh)
{
    m_maxval = 1000;
    m_curval = 0;
    m_displayrect = displayrect;
    m_unbiasedrect = unbias(m_displayrect, wmult, hmult);
}

OSDTypePosSlider::~OSDTypePosSlider()
{
}

void OSDTypePosSlider::Reinit(float wmult, float hmult)
{
    m_displayrect = bias(m_unbiasedrect, wmult, hmult);
    OSDTypeImage::Reinit(wmult, hmult);
}

void OSDTypePosSlider::SetPosition(int pos)
{
    m_curval = pos;
    if (m_curval > 1000)
        m_curval = 1000;
    if (m_curval < 0)
        m_curval = 0;

    int xpos = (int)((m_displayrect.width() / 1000.0) * m_curval);
    int width = m_imagesize.width() / 2;

    xpos = m_displayrect.left() + xpos - width;

    m_displaypos.setX(xpos);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeFillSlider::OSDTypeFillSlider(const QString &name, 
                                     const QString &filename,
                                     QRect displayrect, float wmult, 
                                     float hmult, int scalew, int scaleh)
                 : OSDTypeImage(name, filename, displayrect.topLeft(), wmult, 
                                hmult, scalew, scaleh)
{
    m_maxval = 1000;
    m_curval = 0;
    m_drawwidth = 0;
    m_onlyusefirst = true;
    m_displayrect = displayrect;
    m_unbiasedrect = unbias(m_displayrect, wmult, hmult);
}

OSDTypeFillSlider::~OSDTypeFillSlider()
{
}

void OSDTypeFillSlider::Reinit(float wmult, float hmult)
{
    m_displayrect = bias(m_unbiasedrect, wmult, hmult);
    OSDTypeImage::Reinit(wmult, hmult);
}

void OSDTypeFillSlider::SetPosition(int pos)
{
    m_curval = pos;
    if (m_curval > 1000)
        m_curval = 1000;
    if (m_curval < 0)
        m_curval = 0;

    m_drawwidth = (int)((m_displayrect.width() / 1000.0) * m_curval);
}

void OSDTypeFillSlider::Draw(OSDSurface *surface, int fade, int maxfade, 
                             int xoff, int yoff)
{
    if (!m_isvalid)
        return;

    OSDTypeImage::Draw(surface, fade, maxfade, xoff, yoff);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeEditSlider::OSDTypeEditSlider(const QString &name,
                                     const QString &bluefilename,
                                     const QString &redfilename,
                                     QRect displayrect, float wmult,
                                     float hmult, int scalew, int scaleh)
                 : OSDTypeImage(name)
{
    m_maxval = 1000;
    m_curval = 0;
    m_displayrect = displayrect;
    m_unbiasedrect = unbias(m_displayrect, wmult, hmult);
    m_drawwidth = displayrect.width();

    m_drawMap = new unsigned char[m_drawwidth + 1];
    for (int i = 0; i < m_drawwidth; i++)
         m_drawMap[i] = 0;

    m_displaypos = displayrect.topLeft();
    
    m_yuv = m_alpha = NULL;
    m_isvalid = false;

    m_ryuv = m_ralpha = NULL;
    m_risvalid = false;

    m_redname = redfilename;
    m_bluename = bluefilename;

    m_scalew = scalew;
    m_scaleh = scaleh;
    m_cacheitem = NULL;

    LoadImage(m_redname, wmult, hmult, scalew, scaleh);
    if (m_isvalid)
    {
        m_risvalid = m_isvalid;
        m_ralpha = m_alpha;
        m_ryuv = m_yuv;
        m_rimagesize = m_imagesize;
        m_rybuffer = m_ybuffer;
        m_rubuffer = m_ubuffer;
        m_rvbuffer = m_vbuffer;

        m_isvalid = false;
        m_alpha = m_yuv = NULL;
    }

    LoadImage(m_bluename, wmult, hmult, scalew, scaleh);
}

OSDTypeEditSlider::~OSDTypeEditSlider()
{
    delete [] m_drawMap;
}

void OSDTypeEditSlider::Reinit(float wmult, float hmult)
{
    m_displayrect = bias(m_unbiasedrect, wmult, hmult);
    m_drawwidth = m_displayrect.width();

    delete [] m_drawMap;
    
    m_drawMap = new unsigned char[m_drawwidth + 1];
    for (int i = 0; i < m_drawwidth; i++)
         m_drawMap[i] = 0;

    m_displaypos = m_displayrect.topLeft();

    LoadImage(m_redname, wmult, hmult, m_scalew, m_scaleh);
    if (m_isvalid)
    {
        m_risvalid = m_isvalid;
        m_ralpha = m_alpha;
        m_ryuv = m_yuv;
        m_rimagesize = m_imagesize;
        m_rybuffer = m_ybuffer;
        m_rubuffer = m_ubuffer;
        m_rvbuffer = m_vbuffer;

        m_isvalid = false;
        m_alpha = m_yuv = NULL;
    }

    LoadImage(m_bluename, wmult, hmult, m_scalew, m_scaleh);
}

void OSDTypeEditSlider::ClearAll(void)
{
    for (int i = 0; i < m_drawwidth; i++)
        m_drawMap[i] = 0;
}

void OSDTypeEditSlider::SetRange(int start, int end)
{
    start = (int)((m_drawwidth / 1000.0) * start);
    end = (int)((m_drawwidth / 1000.0) * end);

    if (start < 0)
        start = 0;
    if (start >= m_drawwidth) 
        start = m_drawwidth - 1;
    if (end < 0)
        end = 0;
    if (end >= m_drawwidth)
        end = m_drawwidth - 1;

    if (end < start)
    {
        int tmp = start;
        start = end;
        end = tmp;
    }

    for (int i = start; i < end; i++)
        m_drawMap[i] = 1;
}

void OSDTypeEditSlider::Draw(OSDSurface *surface, int fade, int maxfade,
                             int xoff, int yoff)
{           
    if (!m_isvalid || !m_risvalid)
        return;
            
    unsigned char *dest, *destalpha, *src, *rsrc, *srcalpha, *rsrcalpha;
    unsigned char *udest, *vdest, *usrc, *rusrc, *vsrc, *rvsrc;
            
    int iwidth, riwidth, width;
    iwidth = m_imagesize.width();
    riwidth = m_rimagesize.width();
    width = m_drawwidth;
    int height = m_imagesize.height();

    int ystart = m_displaypos.y();
    int xstart = m_displaypos.x();
            
    xstart += xoff;
    ystart += yoff;

    ystart = (ystart / 2) * 2;
    xstart = (xstart / 2) * 2;

    int startline = 0;
    int startcol = 0;

    if (ystart < 0)
    {
        startline = 0 - ystart;
        ystart = 0;
    }

    if (xstart < 0)
    {
        startcol = 0 - xstart;
        xstart = 0;
    }

    if (height + ystart > surface->height)
        height = surface->height - ystart - 1;
    if (width + xstart > surface->width)
        width = surface->width - xstart - 1;

    if (width == 0 || height == 0)
        return;

    QRect destRect = QRect(xstart, ystart, width, height);
    surface->AddRect(destRect);
 
    int ysrcwidth;              
    int rysrcwidth;
    int ydestwidth;
   
    int uvsrcwidth;
    int ruvsrcwidth;
    int uvdestwidth;
 
    int alphamod = 255;
    
    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    ysrcwidth = startline * iwidth; 
    rysrcwidth = startline * riwidth;
    ydestwidth = ystart * surface->width;

    dest = surface->y + xstart + ydestwidth;
    destalpha = surface->alpha + xstart + ydestwidth;
    src = m_ybuffer + ysrcwidth;
    rsrc = m_rybuffer + rysrcwidth;

    srcalpha = m_alpha + ysrcwidth;
    rsrcalpha = m_ralpha + rysrcwidth;

    uvdestwidth = ydestwidth / 4;
    uvsrcwidth = ysrcwidth / 4;
    ruvsrcwidth = rysrcwidth / 4;

    udest = surface->u + xstart / 2 + uvdestwidth;
    usrc = m_ubuffer + uvsrcwidth;
    rusrc = m_rubuffer + ruvsrcwidth;

    vdest = surface->v + xstart / 2 + uvdestwidth;
    vsrc = m_vbuffer + uvsrcwidth;
    rvsrc = m_rvbuffer + ruvsrcwidth;
    (surface->blendcolumn2func) (rsrc, rusrc, rvsrc, rsrcalpha, riwidth, src,
                                 usrc, vsrc, srcalpha, iwidth,
                                 m_drawMap + startcol, dest, udest, vdest,
                                 destalpha, surface->width, width - startcol,
                                 height - startline, alphamod, 1,
                                 surface->rec_lut, surface->pow_lut);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeBox::OSDTypeBox(const QString &name, QRect displayrect,
                       float wmult, float hmult)
    : OSDType(name), size(displayrect), m_color(Qt::black)
{
    m_unbiasedsize = unbias(size, wmult, hmult);
}

void OSDTypeBox::SetRect(QRect newrect, float wmult, float hmult)
{
    size = newrect;
    m_unbiasedsize = unbias(size, wmult, hmult);
}

OSDTypeBox::OSDTypeBox(const OSDTypeBox &other)
          : OSDType(other.m_name)
{
    size = other.size;
    m_unbiasedsize = other.m_unbiasedsize;
}

OSDTypeBox::~OSDTypeBox()
{
}

void OSDTypeBox::Reinit(float wmult, float hmult)
{
    size = bias(m_unbiasedsize, wmult, hmult);
}

void OSDTypeBox::Draw(OSDSurface *surface, int fade, int maxfade,
                      int xoff, int yoff, unsigned int xalpha)
{
    unsigned char *dest, *destalpha;
    unsigned char alpha = xalpha & 0xff;

    QRect disprect = size;
    disprect.moveBy(xoff, yoff);

    int ystart = disprect.top();
    int yend = disprect.bottom();
    int xstart = disprect.left();
    int xend = disprect.right();

    if (xstart < 0)
        xstart = 0;
    if (xend > surface->width)
        xend = surface->width;
    if (ystart < 0)
        ystart = 0;
    if (yend > surface->height)
        yend = surface->height;

    int height = yend - ystart + 1, width = xend - xstart + 1;

    QRect destRect = QRect(xstart, ystart, width, height); 
    bool needblend = false;

    if (surface->IntersectsDrawn(destRect))
        needblend = true;
    surface->AddRect(destRect);

    int alphamod = 255;
    if (maxfade > 0 && fade >= 0)
        alphamod = (int)((((float)(fade) / maxfade) * 256.0) + 0.5);

    int ydestwidth;

    int h,s,v;
    m_color.getHsv(&h, &s, &v);

    alpha = ((alpha * alphamod) + 0x80) >> 8;
    if (!needblend)
    {
        for (int y = ystart; y < yend; y++)
        {
            ydestwidth = y * surface->width;
            
            memset(surface->y + xstart + ydestwidth, 0, width);
            memset(surface->alpha + xstart + ydestwidth, alpha, width);
        }
        return;
    }
    dest = surface->y + ystart * surface->width + xstart;
    destalpha = surface->alpha + ystart * surface->width + xstart;
    (surface->blendconstfunc) (v, 0, 0, alpha, dest, NULL, NULL, destalpha,
                               surface->width, width, height, 0,
                               surface->rec_lut, surface->pow_lut);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypePositionIndicator::OSDTypePositionIndicator(void)
{
    m_numpositions = 0;
    m_curposition = -1;
    m_offset = 0;
}

OSDTypePositionIndicator::OSDTypePositionIndicator(
                                      const OSDTypePositionIndicator &other)
{
    m_numpositions = other.m_numpositions;
    m_curposition = other.m_curposition;
    m_offset = other.m_offset;
}

OSDTypePositionIndicator::~OSDTypePositionIndicator()
{
}

void OSDTypePositionIndicator::SetPosition(int pos)
{
    m_curposition = pos + m_offset;
    if (m_curposition >= m_numpositions)
        m_curposition = m_numpositions - 1;
}

void OSDTypePositionIndicator::PositionUp(void)
{
    if (m_curposition > m_offset)
        m_curposition--;
    else if (m_curposition == m_offset)
        m_curposition = m_numpositions - 1;
}

void OSDTypePositionIndicator::PositionDown(void)
{
    if (m_curposition < m_numpositions - 1)
        m_curposition++;
    else if (m_curposition == m_numpositions - 1) 
        m_curposition = m_offset;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


OSDTypePositionRectangle::OSDTypePositionRectangle(const QString &name)
                        : OSDType(name), OSDTypePositionIndicator()
{
}       
   
OSDTypePositionRectangle::OSDTypePositionRectangle(
    const OSDTypePositionRectangle &other) 
    : OSDType(other.m_name), OSDTypePositionIndicator(other)
{
    for (int i = 0; i < m_numpositions; i++)
    {
        QRect tmp = other.positions[i];
        positions.push_back(tmp);
    }
    for (int i = 0; i < m_numpositions; i++)
    {
        QRect tmp = other.unbiasedpos[i];
        unbiasedpos.push_back(tmp);
    }
}

OSDTypePositionRectangle::~OSDTypePositionRectangle()
{
}

void OSDTypePositionRectangle::Reinit(float wmult, float hmult)
{
    for (int i = 0; i < m_numpositions; i++)
        positions[i] = bias(unbiasedpos[i], wmult, hmult);
}

void OSDTypePositionRectangle::AddPosition(
    QRect rect, float wmult, float hmult)
{
    positions.push_back(rect);
    unbiasedpos.push_back(unbias(rect, wmult, hmult));
    m_numpositions++;
}

void OSDTypePositionRectangle::Draw(
    OSDSurface *surface, int fade, int maxfade, int xoff, int yoff)
{
    fade = fade;
    maxfade = maxfade;
    xoff = xoff;
    yoff = yoff;

    if (m_curposition < 0 || m_curposition >= m_numpositions)
        return;

    QRect rect = positions[m_curposition];

    unsigned char *src;
    int ystart = rect.top() + yoff;
    int yend = rect.bottom() + yoff;
    int xstart = rect.left() + xoff;
    int xend = rect.right() + xoff;

    if (xstart < 0)
        xstart = 0;
    if (xend > surface->width)
        xend = surface->width;
    if (ystart < 0)
        ystart = 0;
    if (yend > surface->height)
        yend = surface->height;

    int height = yend - ystart + 1, width = xend - xstart + 1;

    QRect destRect = QRect(xstart, ystart, width, height);
    surface->AddRect(destRect);

    for (int y = ystart; y < yend; y++)
    {
        if (y < 0 || y >= surface->height)
            continue;

        for (int x = xstart; x < xstart + 2; x++)
        {
            if (x < 0 || x >= surface->width)
                continue;

            src = surface->y + x + y * surface->width;
            *src = 255;
        }

        for (int x = xend - 2; x < xend; x++)
        {
            if (x < 0 || x >= surface->width)
                continue;

            src = surface->y + x + y * surface->width;
            *src = 255;
        }
    }

    for (int x = xstart; x < xend; x++)
    {
        if (x < 0 || x >= surface->width)
            continue;

        for (int y = ystart; y < ystart + 2; y++)
        {
            if (y < 0 || y >= surface->height)
                continue;

            src = surface->y + x + y * surface->width;
            *src = 255;
        }
        for (int y = yend - 2; y < yend; y++)
        {
            if (y < 0 || y >= surface->height)
                continue;

            src = surface->y + x + y * surface->width;
            *src = 255;
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypePositionImage::OSDTypePositionImage(const QString &name)
    : OSDTypeImage(name), OSDTypePositionIndicator(),
      m_wmult(0.0f), m_hmult(0.0f)
{
}

OSDTypePositionImage::OSDTypePositionImage(const OSDTypePositionImage &other)
                    : OSDTypeImage(other), OSDTypePositionIndicator(other)
{
    m_wmult = other.m_wmult;
    m_hmult = other.m_hmult;

    for (int i = 0; i < m_numpositions; i++)
    {
        positions.push_back(other.positions[i]);
        unbiasedpos.push_back(other.unbiasedpos[i]);
    }
}

OSDTypePositionImage::~OSDTypePositionImage()
{
}

void OSDTypePositionImage::Reinit(float wmult, float hmult)
{
    m_wmult = wmult;
    m_hmult = hmult;
    
    OSDTypeImage::Reinit(wmult, hmult);

    for (int i = 0; i < m_numpositions; i++)
    {
        positions[i] =
            QPoint((int)round(unbiasedpos[i].x() * wmult),
                   (int)round(unbiasedpos[i].y() * hmult));
    }
}

void OSDTypePositionImage::AddPosition(QPoint pos, float wmult, float hmult)
{
    if (m_wmult == 0.0f || m_hmult == 0.0f)
    {
        m_wmult = wmult;
        m_hmult = hmult;
    }
    positions.push_back(pos);
    unbiasedpos.push_back(
        QPoint((int)round(pos.x() / wmult),
               (int)round(pos.y() / hmult)));

    VERBOSE(VB_IMPORTANT,
            "OSDTypePositionImage::AddPosition["<<m_numpositions<<"]("
            <<pos.x()<<"x"<<pos.y()
            <<"  "<<wmult<<", "<<hmult<<")");

    m_numpositions++;
}

void OSDTypePositionImage::Draw(OSDSurface *surface, int fade, int maxfade,
                                int xoff, int yoff)
{
    VERBOSE(VB_IMPORTANT,
            "OSDTypePositionImage::Draw["<<m_curposition<<"]("
            <<m_wmult<<", "<<m_hmult<<")");

    if (m_curposition < 0 || m_curposition >= m_numpositions)
        return;

    QPoint pos = positions[m_curposition];

    OSDTypeImage::SetPosition(pos, m_wmult, m_hmult);
    OSDTypeImage::Draw(surface, fade, maxfade, xoff, yoff);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSDTypeCC::OSDTypeCC(const QString &name, TTFFont *font, int xoff, int yoff,
                     int dispw, int disph, float wmult, float hmult)
         : OSDType(name)
{
    m_font = font;
    m_textlist = NULL;
    xoffset = xoff;
    yoffset = yoff;
    displaywidth = dispw;
    displayheight = disph;
    m_wmult = wmult;
    m_hmult = hmult;

    QRect rect = QRect(0, 0, 0, 0);
    m_box = new OSDTypeBox("cc_background", rect, wmult, hmult);
    m_ccbackground = gContext->GetNumSetting("CCBackground", 0);
}

OSDTypeCC::~OSDTypeCC()
{
    ClearAllCCText();
    delete m_box;
}

void OSDTypeCC::Reinit(float wmult, float hmult)
{
    (void) wmult;
    (void) hmult;
    VERBOSE(VB_IMPORTANT, "Programmer error: "
            "Call to OSDTypeCC::Reinit(float,float)");
}

void OSDTypeCC::Reinit(int x, int y, int dispw, int disph,
                       float wmult, float hmult)
{
    xoffset = x;
    yoffset = y;
    displaywidth = dispw;
    displayheight = disph;
    m_wmult = wmult;
    m_hmult = hmult;    
}

void OSDTypeCC::AddCCText(const QString &text, int x, int y, int color, 
                          bool teletextmode)
{
    ccText *cc = new ccText();
    cc->text = text;
    cc->x = x;
    cc->y = y;
    cc->color = color;
    cc->teletextmode = teletextmode;

    if (!m_textlist)
        m_textlist = new vector<ccText *>;

    m_textlist->push_back(cc);
}

void OSDTypeCC::ClearAllCCText()
{
    if (m_textlist)
    {
        vector<ccText*>::iterator i = m_textlist->begin();
        for (; i != m_textlist->end(); i++)
        {
            ccText *cc = (*i);
            if (cc)
                delete cc;
        }
        delete m_textlist;
        m_textlist = NULL;
    }
}

int OSDTypeCC::UpdateCCText(vector<ccText*> *ccbuf,
                            int replace, int scroll, bool scroll_prsv,
                            int scroll_yoff, int scroll_ymax)
// ccbuf      :  new text
// replace    :  replace last lines
// scroll     :  scroll amount
// scroll_prsv:  preserve last lines and move into scroll window
// scroll_yoff:  yoff < scroll window <= ymax
// scroll_ymax:
{
    vector<ccText*>::iterator i;
    int visible = 0;

    if (m_textlist && (scroll || replace))
    {
        ccText *cc;

        // get last row
        int ylast = 0;
        i = m_textlist->end() - 1;
        cc = *i;
        if (cc)
            ylast = cc->y;

        // calculate row positions to delete, keep
        int ydel = scroll_yoff + scroll;
        int ykeep = scroll_ymax;
        int ymove = 0;
        if (scroll_prsv && ylast)
        {
            ymove = ylast - scroll_ymax;
            ydel += ymove;
            ykeep += ymove;
        }

        i = m_textlist->begin();
        while (i < m_textlist->end())
        {
            cc = (*i);
            if (!cc)
            {
                i = m_textlist->erase(i);
                continue;
            }

            if (cc->y > (ylast - replace))
            {
                // delete last lines
                delete cc;
                i = m_textlist->erase(i);
            }
            else if (scroll)
            {
                if (cc->y > ydel && cc->y <= ykeep)
                {
                    // scroll up
                    cc->y -= (scroll + ymove);
                    i++;
                }
                else
                {
                    // delete lines outside scroll window
                    i = m_textlist->erase(i);
                    delete cc;
                }
            }
            else
                i++;
        }
    }

    if (m_textlist)
        visible += m_textlist->size();

    if (ccbuf)
    {
        // add new text
        i = ccbuf->begin();
        while (i < ccbuf->end())
        {
            if (*i)
            {
                visible++;
                if (!m_textlist)
                    m_textlist = new vector<ccText *>;
                m_textlist->push_back(*i);
            }
            i++;
        }
    }

    return visible;
}

void OSDTypeCC::Draw(OSDSurface *surface, int fade, int maxfade, int xoff, 
                     int yoff)
{
    // not used
    fade = fade;
    maxfade = maxfade;
    xoff = xoff;
    yoff = yoff;

    vector<ccText*>::iterator i = m_textlist->begin();
    for (; i != m_textlist->end(); i++)
    {
        ccText *cc = (*i);

        if (cc && (cc->text != QString::null))
        {
            int textlength = 0;
            m_font->CalcWidth(cc->text, &textlength);

            int x, y;
            if (cc->teletextmode)
            {
                // position as if we use a fixed size font
                // on a 24 row / 40 char grid (teletext expects us to)
                x = cc->y * displaywidth / 40 + xoffset;
                y = cc->x * displayheight / 25 + yoffset;
            }
            else
            {
                x = (cc->x + 3) * displaywidth / 36 + xoffset;
                y = cc->y * displayheight / 17 + yoffset;
            }

            int maxx = x + textlength;
            int maxy = y + m_font->Size() * 3 / 2;

            if (maxx > surface->width)
                maxx = surface->width;

            if (maxy > surface->height)
                maxy = surface->height;

            if (m_ccbackground && !cc->teletextmode)
            {
                QRect rect = QRect(0, 0, textlength + 4, 
                                   (m_font->Size() * 3 / 2) + 3);
                m_box->SetRect(rect, m_wmult, m_hmult);
                m_box->Draw(surface, 0, 0, x - 2, y - 2);
            }

            m_font->setOutline(TRUE);

            switch (cc->color)
            {
                case 1: m_font->setColor((Qt::red), kTTF_Normal); break;
                case 2: m_font->setColor((Qt::green), kTTF_Normal); break;
                case 3: m_font->setColor((Qt::yellow), kTTF_Normal); break;
                case 4: m_font->setColor((Qt::blue), kTTF_Normal); break;
                case 5: m_font->setColor((Qt::magenta), kTTF_Normal); break;
                case 6: m_font->setColor((Qt::cyan), kTTF_Normal); break;
                case 7: m_font->setColor((Qt::white), kTTF_Normal); break;
                default: m_font->setColor((Qt::white), kTTF_Normal); break;
            }

            m_font->DrawString(surface, x, y + 2, cc->text, maxx, maxy, 255); 
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void OSDType708CC::Reinit(int x, int y, int dispw, int disph)
{
    xoffset = x;
    yoffset = y;
    displaywidth = dispw;
    displayheight = disph;
}

OSDType708CC::OSDType708CC(const QString &name, TTFFont *fonts[48],
			   int xoff, int yoff, int dispw, int disph) : 
    OSDType(name)
{
    xoffset = xoff;
    yoffset = yoff;
    displaywidth  = dispw;
    displayheight = disph;

    for (uint i = 0; i < 48; i++)
        m_fonts[i] = fonts[i];
}

QRect OSDType708CC::CalcBounds(const OSDSurface *surface,
                               const CC708Window &win,
                               const vector<CC708String*> &list,
                               uint &min_xoffset)
{
    uint max_width = 0, total_height = 0, i = 0;
    min_xoffset = 0xffffffff;
    for (uint row = 0; (row < win.true_row_count) && (i < list.size()); row++)
    {
        uint tot_width = 0, max_height = 0;
        for (; (i < list.size()) && list[i] && (list[i]->y <= row); i++)
        {
            int space_length, text_length;
            if (list[i]->y < row)
                continue;

            TTFFont *font = m_fonts[list[i]->attr.FontIndex()];

            if (list[i]->str.stripWhiteSpace().isEmpty())
            {
                max_height = max(max_height, (uint)font->Size() * 3 / 2);
                continue;
            }

            font->CalcWidth(list[i]->str, &text_length);
            font->CalcWidth(" ", &space_length);

            min_xoffset = min(min_xoffset, space_length * list[i]->x);
            tot_width  += max(text_length, 0);
            max_height  = max(max_height, (uint)font->Size() * 3 / 2);

            VERBOSE(VB_VBI, "Row#"<<row<<" str#"<<i<<" "
                    <<"w/x("<<list[i]->x<<"): "
                    <<"W,H ("<<text_length<<","<<max_height<<") "
                    <<"'"<<list[i]->str<<"'"
                    <<" xoff: "<<space_length * list[i]->x);
        }
        max_width     = max(max_width, tot_width);
        total_height += max_height;
    }
    min_xoffset = (min_xoffset == 0xffffffff) ? 0 : min_xoffset;

    if (!max_width || !total_height)
        return QRect(0,0,0,0);

    // add some padding..
    max_width    += 4;
    total_height += 4;
    //VERBOSE(VB_VBI, "Window size: ("<<max_width<<", "<<total_height<<")");

    // 16:9
    float xrange = 210.0f;
    float yrange = 75.0f; 
    // 4:3
    //float xrange = 160.0f;
    //float yrange = 75.0f;

    // relative position
    xrange = (win.relative_pos) ? 100.0f : xrange;
    yrange = (win.relative_pos) ? 100.0f : yrange;

    float xmult = (surface->width  - 4) / xrange;
    float ymult = (surface->height - 4) / yrange;

    uint anchor_x = (uint) xmult * win.anchor_horizontal + 2;
    uint anchor_y = (uint) ymult * win.anchor_vertical   + 2;
    //VERBOSE(VB_VBI, "Raw Anchor loc:  ("
    //        <<anchor_x<<", "<<anchor_y<<")");


    bool center = (win.anchor_point % 3 == 1);
    bool right  = (win.anchor_point % 3 == 2);
    bool middle = (win.anchor_point / 3 == 1);
    bool bottom = (win.anchor_point / 3 == 2);
    if (center)
        anchor_x = max(0, ((int)anchor_x) - (((int)max_width) / 2));
    if (right)
        anchor_x = max(0, ((int)anchor_x) - ((int)max_width));
    if (middle)
        anchor_y = max(0, ((int)anchor_y) - (((int)total_height) / 2));
    if (bottom)
        anchor_y = max(0, ((int)anchor_y) - ((int)total_height));
    //VERBOSE(VB_VBI, "Adj Anchor loc:  ("
    //        <<anchor_x<<", "<<anchor_y<<")");


    // Make sure bounds are within window by adjusting anchor point
    int x_adj = (int)(anchor_x + max_width) - (surface->width  - 4);
    if (x_adj > 0)
        anchor_x = max(0, (int)anchor_x - x_adj);

    int y_adj = (int)(anchor_y + total_height) - (surface->height  - 4);
    if (y_adj > 0)
        anchor_y = max(0, (int)anchor_y - y_adj);
    //VERBOSE(VB_VBI, "Bnd Anchor loc:  ("
    //        <<anchor_x<<", "<<anchor_y<<")");

    // If it is still out of bounds, shrink the surface
    if (anchor_x + max_width > (uint)surface->width)
        max_width = surface->width - anchor_x;

    if (anchor_y + total_height > (uint)surface->height)
        total_height = surface->height - anchor_y;

    return QRect(anchor_x, anchor_y, max_width, total_height);
}

void OSDType708CC::Draw(OSDSurface *surface,
                        const QPoint &ul, /* upper left corner */
                        const CC708Window &win,
                        const vector<CC708String*> &list)
{
    int maxx = surface->width;
    int maxy = surface->height;
    uint max_width = 0, total_height = 0, i = 0;
    for (uint row = 0; (row < win.true_row_count) && (i < list.size()); row++)
    {
        uint tot_width = 0, max_height = 0;
        for (; (i < list.size() && list[i] && (list[i]->y == row)); i++)
        {
            int text_length;

            if (list[i]->str.isEmpty())
                continue;

            TTFFont *font = m_fonts[list[i]->attr.FontIndex()];

            font->CalcWidth(list[i]->str, &text_length);

            font->setColor(list[i]->attr.GetFGColor(), kTTF_Normal);

            font->setOutline(false);
            font->setShadow(0,0);
            if (list[i]->attr.edge_type == 3/*kEdgeUniform*/)
            {
                font->setColor(list[i]->attr.GetEdgeColor(), kTTF_Outline);
                font->setOutline(true);
            }
            else if (list[i]->attr.edge_type == 4/*kEdgeLeftShadow*/)
            {
                font->setColor(list[i]->attr.GetEdgeColor(), kTTF_Shadow);
                font->setShadow(-2, +2);
            }
            else if (list[i]->attr.edge_type == 5/*kEdgeRightShadow*/)
            {
                font->setColor(list[i]->attr.GetEdgeColor(), kTTF_Shadow);
                font->setShadow(+2, +2);
            }

            font->DrawString(surface,
                             ul.x() + tot_width, ul.y() + total_height + 2,
                             list[i]->str, maxx, maxy,
                             list[i]->attr.GetFGAlpha());

            tot_width  += max(text_length, 0);
            max_height  = max(max_height, (uint)font->Size() * 3 / 2);
        }
        max_width     = max(max_width, tot_width);
        total_height += max_height;
    }
}

void OSDType708CC::Draw(OSDSurface *surface, int /*fade*/, int /*maxfade*/,
                        int /*xoff*/, int /*yoff*/)
{
    float wmult = 1.0f;
    float hmult = 1.0f;

    if (!cc708data || surface->width < 4 || surface->height < 4)
        return;

    for (uint i = 0; i < 8; i++)
    {
        const CC708Window &win = cc708data->windows[i];
        if (!win.exists)
            continue;

        QMutexLocker locker(&win.lock);

        //VERBOSE(VB_VBI, "Window #"<<i);
        vector<CC708String*> list = win.GetStrings();
        uint box_xoffset = 0;
        QRect bounds = CalcBounds(surface, win, list, box_xoffset);
        QPoint ul(0,0);
        if (bounds.width())
        {
            if (list.size() && win.GetFillAlpha())
            {
                QRect rect(0,0, bounds.width(), bounds.height());
                OSDTypeBox box(QString("cc708_background%1").arg(i),
                               rect, wmult, hmult);
                box.SetRect(rect, wmult, hmult);
                box.Draw(surface, 0/*fade*/, 0/*maxfade*/,
                         bounds.left() + box_xoffset/*xoff*/,
                         bounds.top()/*yoff*/, win.GetFillAlpha());
            }
            Draw(surface, bounds.topLeft(), win, list);
        }
        //VERBOSE(VB_VBI, "");

        for (uint i = 0; i < list.size(); i++)
            delete list[i];
    }
}
