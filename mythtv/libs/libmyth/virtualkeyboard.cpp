#include <qpixmap.h>
#include <qimage.h>
#include <qapplication.h>

#include "virtualkeyboard.h"
#include "mythcontext.h"
#include "mythdialogs.h"
#include "uitypes.h"

#define LOC      QString("VirtualKeyboard: ")
#define LOC_WARN QString("VirtualKeyboard, Warning: ")
#define LOC_ERR  QString("VirtualKeyboard, Error: ")

VirtualKeyboard::VirtualKeyboard(MythMainWindow *parent,
                    QWidget *parentEdit,
                    const char *name,
                    bool setsize)
            : MythThemedDialog(parent, name, setsize)
{
    setFrameStyle(QFrame::Panel | QFrame::Raised);
    setLineWidth(1);
    m_parentEdit = parentEdit;

    SwitchLayout(gContext->GetLanguageAndVariant());
}

void VirtualKeyboard::SwitchLayout(const QString &lang)
{
    if (!m_parentEdit)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No edit receiving output");
        reject();
        return;
    }

    QString language = lang.lower();

    // figure out which of the english layouts to use (UK or US)
    if (language.left(2) == "en")
    {
        if (language.contains("en_gb", false))
        {
            language = "en_uk";
        }
        else
        {
            language = "en_us";
        }
    }

    QString theme_file = QString("keyboard/%1_").arg(language);

    if (!loadThemedWindow("keyboard", theme_file))
    {
        VERBOSE(VB_GENERAL, LOC_WARN + 
                QString("Cannot find layout for '%1'").arg(language));

        // cannot find layout so fallback to US English layout
        if (!loadThemedWindow("keyboard", "keyboard/en_us_"))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + 
                    "Cannot find layout for US English");

            reject();
            return;
        }
    }

    // get dialog size from keyboard_container area
    LayerSet *container = getContainer("keyboard_container");

    if (!container)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + 
                "Cannot find the 'keyboard_container' in your theme");

        reject();
        return;
    }

    m_popupWidth = container->GetAreaRect().width();
    m_popupHeight = container->GetAreaRect().height();
    setFixedSize(QSize(m_popupWidth, m_popupHeight));

    QWidget *pw = m_parentEdit;
    QWidget *tlw = pw->topLevelWidget();
    QRect pwg = pw->geometry();
    QRect tlwg = tlw->frameGeometry();

    QPoint newpos;

    PopupPosition preferredPos;
    if (m_parentEdit->inherits("MythLineEdit"))
    {
        MythLineEdit *par = (MythLineEdit *)m_parentEdit;
        preferredPos = par->getPopupPosition();
    }
    else if (m_parentEdit->inherits("MythRemoteLineEdit"))
    {
        MythRemoteLineEdit *par = (MythRemoteLineEdit *)m_parentEdit;
        preferredPos = par->getPopupPosition();
    }
    else if (m_parentEdit->inherits("MythComboBox"))
    {
        MythComboBox *par = (MythComboBox *)m_parentEdit;
        preferredPos = par->getPopupPosition();
    }
    else
    {
        preferredPos = VK_POSCENTERDIALOG;
    }

    if (preferredPos == VK_POSBELOWEDIT)
    {
        if (pw->mapTo(tlw, QPoint(0,pwg.height() + m_popupHeight + 5)).y()
                < tlwg.height())
        {
            newpos = QPoint(pwg.width() / 2 - m_popupWidth / 2, pwg.height() + 5);
        }
        else 
        {
            newpos = QPoint(pwg.width() / 2 - m_popupWidth / 2, - 5 - m_popupHeight);
        }
    }
    else if (preferredPos == VK_POSABOVEEDIT)
    {
        if (pw->mapTo(tlw, QPoint(0, - m_popupHeight - 5)).y()
            > 0)
        {
            newpos = QPoint(pwg.width() / 2 - m_popupWidth / 2, - 5 - m_popupHeight);
        }
        else 
        {
            newpos = QPoint(pwg.width() / 2 - m_popupWidth / 2, pwg.height() + 5);
        }
    }
    else if (preferredPos == VK_POSTOPDIALOG)
    {
        newpos = QPoint(tlwg.width() / 2 - m_popupWidth / 2, 5);
        this->move(newpos);
    }
    else if (preferredPos == VK_POSBOTTOMDIALOG)
    {
        newpos = QPoint(tlwg.width() / 2 - m_popupWidth / 2, 
                        tlwg.height() - 5 - m_popupHeight);
        this->move(newpos);
    }
    else if (preferredPos == VK_POSCENTERDIALOG)
    {
        newpos = QPoint(tlwg.width() / 2 - m_popupWidth / 2, 
                        tlwg.height() / 2 - m_popupHeight / 2);
        this->move(newpos);
    }

    if (preferredPos == VK_POSABOVEEDIT || preferredPos == VK_POSBELOWEDIT)
    {
        int delx = pw->mapTo(tlw,newpos).x() + m_popupWidth - tlwg.width() + 5;
        newpos = QPoint(newpos.x() - (delx > 0 ? delx : 0), newpos.y());
        delx = pw->mapTo(tlw, newpos).x();
        newpos = QPoint(newpos.x() - (delx < 0 ? delx : 0), newpos.y());

        int xbase, width, ybase, height;
        float wmult, hmult;
        gContext->GetScreenSettings(xbase, width, wmult, ybase, height, hmult);
        newpos.setX(newpos.x() - xbase);
        newpos.setY(newpos.y() - ybase);

        this->move( pw->mapToGlobal( newpos ) );
    }

    // find the UIKeyboardType
    m_keyboard = getUIKeyboardType("keyboard");
    if (!m_keyboard)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Cannot find the UIKeyboardType in your theme");

        reject();
        return;
    }

    if (m_parentEdit->inherits("QComboBox"))
    {
        QComboBox *combo = (QComboBox *) m_parentEdit;
        m_keyboard->SetEdit(combo->lineEdit());
    }
    else
        m_keyboard->SetEdit(m_parentEdit);

    m_keyboard->SetParentDialog(this);
}

VirtualKeyboard::~VirtualKeyboard(void)
{
    Teardown();
}

void VirtualKeyboard::deleteLater(void)
{
    Teardown();
}

void VirtualKeyboard::Teardown(void)
{
    m_keyboard   = NULL;
    m_parentEdit = NULL;
}

void VirtualKeyboard::Show(void)
{
    grabKeyboard();

    MythDialog::Show();

    if (m_parentEdit)
        m_parentEdit->setFocus();
}

void VirtualKeyboard::hide()
{
    releaseKeyboard();

    if (m_parentEdit)
        m_parentEdit->setFocus();

    MythDialog::hide();
}

void VirtualKeyboard::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions, false))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;
            if (action == "ESCAPE")
                accept();
            else
                handled = false;
        }
    }

    //just pass all unhandled key events for the keyboard to handle
    if (!handled && m_keyboard)
        m_keyboard->keyPressEvent(e);
}

