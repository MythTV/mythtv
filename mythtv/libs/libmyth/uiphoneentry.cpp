#include <iostream>
using namespace std;

#include <qpixmap.h>
#include <qimage.h>
#include <qapplication.h>

#include "uiphoneentry.h"
#include "mythcontext.h"
#include "mythdialogs.h"
#include "dialogbox.h"
#include "uitypes.h"

UIPhoneEntry::UIPhoneEntry(MythMainWindow *parent, MythDialog *bg, 
                           const char *name)
            : MythDialog(parent, name)
{
    m_active = false;
    m_lastKey = "";
    m_lastIndex = 0;
    m_drawBlink = false;
    m_inDelay = false;
    m_capsLock = false;
    m_bg = bg;
    m_bgBackup = 0;
    m_buttonOffset = QPoint(0, 0);

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    m_trans = gContext->GetNumSetting("PlayBoxTransparency", 1);

    m_theme = new XMLParse();
    m_theme->SetWMult(wmult);
    m_theme->SetHMult(hmult);

    QDomElement xmldata;

    if (!m_theme->LoadTheme(xmldata, "phoneentry"))
    {
        DialogBox diag(gContext->GetMainWindow(), "The theme you are using "
                       "does not contain a 'phoneentry' element.  Please "
                       "contact the theme creator and ask if they could "
                       "please update it.");
        diag.AddButton("OK");
        diag.exec();
   
        reject(); 
        return;
    }

    loadWindow(xmldata);

    m_lineEdit = new QLineEdit(this, "phoneedit");
    m_lineEdit->setFocus();
    m_lineEdit->setFrame(false);
    m_lineEdit->setText("");
    m_lineEdit->setGeometry(QRect(28, 28, 199 ,26));

    fontProp *entryFont = m_theme->GetFont("entry");
    if (entryFont)
    {
        m_lineEdit->setFont(entryFont->face);
        m_lineEdit->setPaletteForegroundColor(entryFont->color);
    }
    else
    {
        m_lineEdit->setFont(gContext->GetMediumFont());
        m_lineEdit->setPaletteForegroundColor("#ffffff");
    }

    LayerSet *container = m_theme->GetSet("phone");
    if (container)
    {
        UIType *entry = (UIType*)container->GetType("entryline");
        if (entry)
        {
            // the trans version of the phoneshell has a drop shadow
            // the solid version doesn't and is smaller and so
            // interior items need to be moved over a bit to match up
            if (!m_trans)
            {
                QRect r = entry->getScreenArea();
                r.moveTopLeft(r.topLeft() - QPoint(5, 5));
                m_lineEdit->setGeometry(r);
            }
            else
                m_lineEdit->setGeometry(entry->getScreenArea());
        }

        UIImageType *img;
        img = (UIImageType*)container->GetType("background");
        if (img)
        {
            m_phoneRect.setSize(QSize(img->GetImage().width(), 
                                      img->GetImage().height()));
        }
        UIKeyboardType *kbd = (UIKeyboardType*)container->GetType("phonekbd");
        if (kbd)
        {
            img = (UIImageType*)kbd->GetType("clicked");
            if (img)
                m_buttonSize = img->GetImage().size();
           
            // move subtitle images over if using solid box 
            if (!m_trans)
            {
                img = (UIImageType*)kbd->GetType("delete");
                img->SetPosition(img->DisplayPos() - QPoint(5, 5));

                img = (UIImageType*)kbd->GetType("space");
                img->SetPosition(img->DisplayPos() - QPoint(5, 5));

                img = (UIImageType*)kbd->GetType("caps");
                img->SetPosition(img->DisplayPos() - QPoint(5, 5));
            }

            UIKeyboardType::KeyList keys = kbd->GetKeys();
            UIKeyboardType::KeyList::iterator it;
            for (it = keys.begin(); it != keys.end(); ++it)
            {
                UIKeyType *key = (*it);
                QPoint pos = key->GetPosition();

                // move key buttons over if using solid box 
                if (!m_trans)
                {
                    pos -= QPoint(5, 5);
                    key->SetPosition(pos);
                } 
                QRect r(pos, m_buttonSize);
                m_buttonsRect |= r;

               
                // move text items over if using solid box 
                if (!m_trans)
                { 
                    UITextType *text;
                    QRect rect;
                    text = (UITextType*)key->GetType("label");
                    rect = text->DisplayArea();
                    rect.moveTopLeft(rect.topLeft() - QPoint(5, 5));
                    text->SetDisplayArea(rect);

                    text = (UITextType*)key->GetType("subtitle");
                    rect = text->DisplayArea();
                    rect.moveTopLeft(rect.topLeft() - QPoint(5, 5));
                    text->SetDisplayArea(rect);
                }
            }
        }
    }

    setFrameStyle(QFrame::Box | QFrame::Plain);

/* FIXME: better solution than 2 new keys?
    MythMainWindow *mainwindow = gContext->GetMainWindow();
    mainwindow->RegisterKey("Phone Entry", "BACKSPACE", "Backspace", "B");
    mainwindow->RegisterKey("Phone Entry", "CAPSLOCK", "Caps lock", "G");
*/
}

UIPhoneEntry::~UIPhoneEntry(void)
{
    if (m_bgBackup)
        delete m_bgBackup;
}

void UIPhoneEntry::Show(int x, int y)
{
    if (m_active)
        return;
    m_active = true;

    m_lineEdit->installEventFilter(this);

    // calculate position on screen
    int w = m_phoneRect.size().width();
    int h = m_phoneRect.size().height();

    if (x == -1)
        x = (screenwidth - w) / 2;
    else
        x = (int)(x * wmult);
    if (y == -1)
        y = (screenheight - h) / 2;
    else
        y = (int)(y * hmult);

    // gray out background
    if (m_flags & PH_GRAYBG)
    {
        QPainter darken;
        darken.begin(m_bg);
        grayOut(&darken, m_bg->size());
        darken.end();
    }

    QPixmap background(w, h);
    if (m_flags & PH_TRANS)
        background = QPixmap::grabWindow(m_bg->winId(), x, y, w, h);
    else
        background.fill(this, x, y);
    setPaletteBackgroundPixmap(background);

    setFixedSize(w, h);
    setGeometry(x, y, w, h);

    MythDialog::Show();
}

void UIPhoneEntry::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    QRect r = e->rect();
    
    if (!r.intersects(m_phoneRect))
        return;

    QPixmap pix(r.size());
    pix.fill(this, r.topLeft());
    QPainter tmp(&pix);
    tmp.translate(-r.x(), -r.y());

    LayerSet *container = m_theme->GetSet("phone");

    if (container) 
    {
        if (m_drawBlink)
        {
            UIKeyboardType *kbd = 
                (UIKeyboardType*)container->GetType("phonekbd");
            if (kbd)
            {
                UIImageType *img = (UIImageType*)kbd->GetType("clicked");
                if (img)
                    img->SetPosition(m_buttonOffset);
            }
        }

        // draw phone shell
        container->Draw(&tmp, 0, m_drawBlink);

        // draw text entry 
        container->Draw(&tmp, 1, m_drawBlink);

        // draw phone interior elements
        container->Draw(&tmp, 2, m_drawBlink);

        tmp.end();
        p.drawPixmap(r.topLeft(), pix);
        // bitBlt( this, r.topLeft(), &tmp );
    }
}

void UIPhoneEntry::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_blinkId)
    {
        update(QRect(m_buttonOffset, m_buttonSize));
        m_drawBlink = false;
        m_blinkId = -1;
    }
    if (e->timerId() == m_delayId)
    {
        m_lineEdit->cursorForward(false);
        m_inDelay = false;
        m_delayId = -1;
    }
    killTimer(e->timerId());
}

bool UIPhoneEntry::eventFilter(QObject *o, QEvent *e)
{
    if (!m_active) {
        qApp->sendEvent(m_bg, e); 
        return QWidget::eventFilter(o, e);
    }
        
    if (o == m_lineEdit && e->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = (QKeyEvent*)e;
        QStringList actions;
        gContext->GetMainWindow()->TranslateKeyPress("Phone Entry", ke, 
                                                     actions);

        bool handled = false;
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];

            if (action == "ESCAPE")
            {
                m_lineEdit->setText("**CANCEL**");
                m_active = false;
                accept();
                return(true);
            }

            if (action == "SELECT")
            {
                m_active = false;
                accept();
                return(true);
            }

            LayerSet *container = m_theme->GetSet("phone");
            UIKeyboardType *kbd;
            kbd = (UIKeyboardType*)container->GetType("phonekbd");
            if (kbd)
            {
                if (kbd->HasKey(action))
                {
                    keyClicked(action);
                    return(true);
                }
                else 
                    qApp->sendEvent(m_bg, e); 
            }
        }
    }

    return QWidget::eventFilter(o, e);
}

void UIPhoneEntry::done(int r)
{
    MythDialog::done(r);
}

void UIPhoneEntry::grayOut(QPainter *tmp, QSize size)
{
    int transparentFlag = gContext->GetNumSetting("PlayBoxShading", 0);

    if (transparentFlag == 0)
        tmp->fillRect(QRect(QPoint(0, 0), size),
                      QBrush(QColor(10, 10, 10), Dense4Pattern));
    else if (transparentFlag == 1)
        tmp->drawPixmap(0, 0, *m_bgBackup, 0, 0, (int)(800*wmult),
                        (int)(600*hmult));
}       

void UIPhoneEntry::keyClicked(QString action)
{
    LayerSet *container = m_theme->GetSet("phone");
    UIKeyboardType *kbd = (UIKeyboardType*)container->GetType("phonekbd");
    UIKeyType *keydata = kbd->GetKey(action);
    QString key = keydata->Name();
    QPoint pos = keydata->GetPosition();
    QString chars = keydata->GetChars();

    //  key hit, switch caps
    if (action == "CAPSLOCK" || action == "UP")
    {
        m_lastKey = key;
        m_capsLock = !m_capsLock;
        switchCaps();
        keyBlink(pos);
        update(m_buttonsRect);
        return;
    }

    // backspace key hit
    if (action == "BACKSPACE" || action == "LEFT")
    {
        m_lastKey = key;

        if (m_inDelay)
        {
            m_lineEdit->cursorForward(false);
            m_inDelay = false;
            killTimer(m_delayId);
        }

        m_lineEdit->backspace();
        keyBlink(pos);

        emit valueChanged(m_lineEdit->text());
        return;
    }

    // we've hit a new key, kill delay timer and move 
    // cursor over to indicate we're done with last keystroke
    if (m_inDelay && key != m_lastKey)
    {
        m_lineEdit->cursorForward(false);
        m_inDelay = false;
        killTimer(m_delayId);
    }

    // we've hit the same key again, cycle to next character
    // associated with this key
    if (m_inDelay && key == m_lastKey)
    {
        m_lastIndex = (m_lastIndex+1) % chars.length();
        m_lineEdit->del();
        m_lineEdit->insert(!m_capsLock ? chars[m_lastIndex] :                                                            chars[m_lastIndex].upper());
        m_lineEdit->cursorBackward(false);

        killTimer(m_delayId);
        m_delayId = startTimer(2000);

        keyBlink(pos);

        emit valueChanged(m_lineEdit->text());
        return;
    }

    // otherwise we've hit this key for the first time and we're not
    // in the delay timer.
    m_inDelay = true;
    m_lastKey = key;
    m_lastIndex = 0; 
    m_lineEdit->insert(!m_capsLock ? chars[m_lastIndex] :                                                            chars[m_lastIndex].upper());
    m_lineEdit->cursorBackward(false);

    if (chars.length() > 1)
        m_delayId = startTimer(3000);
    else
        m_delayId = startTimer(0);
    keyBlink(pos);

    emit valueChanged(m_lineEdit->text());
}

void UIPhoneEntry::keyBlink(const QPoint &pos)
{
    if (m_drawBlink)
        return;

    m_buttonOffset = QPoint(pos.x(), pos.y());
    m_drawBlink = true;
    update(QRect(m_buttonOffset, m_buttonSize));
    m_blinkId = startTimer(150);
}

void UIPhoneEntry::switchCaps()
{
    LayerSet *container = m_theme->GetSet("phone");
    UIKeyboardType *kbd = (UIKeyboardType*)container->GetType("phonekbd");

    UIKeyboardType::KeyList keys = kbd->GetKeys();
    UIKeyboardType::KeyList::iterator it;
    for (it = keys.begin(); it != keys.end(); ++it)
    {
        UIKeyType *key = (*it);

        UITextType *subtitle = (UITextType*)key->GetType("subtitle");
        QString text = subtitle->GetText();

        if (!(text.isNull() || text.isEmpty()))
        {
            if (m_capsLock)
                subtitle->SetText(text.upper());
            else
                subtitle->SetText(text.lower());
        }
    }
}

void UIPhoneEntry::loadWindow(QDomElement &element)
{
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
                m_theme->parseFont(e);
            else if (e.tagName() == "container")
            {
                QRect area;
                QString name;
                int context; 
                m_theme->parseContainer(e, name, context, area);
            }
        }
    }
}
