#include <qcursor.h>
#include <qdialog.h>
#include <qdir.h>
#include <qvbox.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qdir.h>
#include <qregexp.h>
#include <qaccel.h>
#include <qfocusdata.h>
#include <qdict.h>
#include <qsqldatabase.h>
#include <qobjectlist.h> 
#include <qeventloop.h>
#include <qdeepcopy.h>

#ifdef QWS
#include <qwindowsystem_qws.h>
#endif

#include <iostream>
#include <pthread.h>
using namespace std;

#ifdef USE_LIRC
#include "lirc.h"
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenu.h"
#include "jsmenuevent.h"
#endif

#include "uitypes.h"
#include "uilistbtntype.h"
#include "xmlparse.h"
#include "mythdialogs.h"
#include "lcddevice.h"
#include "mythmediamonitor.h"
#include "screensaver.h"
#include "mythdbcon.h"

#ifdef USING_MINGW
#undef LoadImage
#endif

/** \class MythDialog
 *  \brief Base dialog for most dialogs in MythTV using the old UI
 */

MythDialog::MythDialog(MythMainWindow *parent, const char *name, bool setsize)
    : QFrame(parent, name), rescode(kDialogCodeAccepted)
{
    if (!parent)
    {
        cerr << "Trying to create a dialog without a parent.\n";
        return;
    }

    in_loop = false;

    gContext->GetScreenSettings(xbase, screenwidth, wmult,
                                ybase, screenheight, hmult);

    defaultBigFont = gContext->GetBigFont();
    defaultMediumFont = gContext->GetMediumFont();
    defaultSmallFont = gContext->GetSmallFont();

    setFont(defaultMediumFont);

    if (setsize)
    {
        move(0, 0);
        setFixedSize(QSize(screenwidth, screenheight));
        gContext->ThemeWidget(this);
    }

    parent->attach(this);
    m_parent = parent;
}

MythDialog::~MythDialog()
{
    TeardownAll();
}

void MythDialog::deleteLater(void)
{
    hide();
    TeardownAll();
    QFrame::deleteLater();
}

void MythDialog::TeardownAll(void)
{
    if (m_parent)
    {
        m_parent->detach(this);
        m_parent = NULL;
    }
}

void MythDialog::setNoErase(void)
{
    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
#ifdef QWS
    flags |= WResizeNoErase;
#endif
    setWFlags(flags);
}

bool MythDialog::onMediaEvent(MythMediaDevice*)
{
    return false;
}



void MythDialog::Show(void)
{
    show();
}

void MythDialog::setResult(DialogCode r)
{
    if ((r < kDialogCodeRejected) ||
        ((kDialogCodeAccepted < r) && (r < kDialogCodeListStart)))
    {
        VERBOSE(VB_IMPORTANT, "Programmer Error: MythDialog::setResult("
                <<r<<") called with invalid DialogCode");
    }

    rescode = r;
}

void MythDialog::done(int r)
{
    hide();
    setResult((DialogCode) r);
    close();
}

void MythDialog::AcceptItem(int i)
{
    if (i < 0)
    {
        VERBOSE(VB_IMPORTANT, "Programmer Error: MythDialog::AcceptItem("
                <<i<<") called with negative index");
        reject();
        return;
    }

    done((DialogCode)((int)kDialogCodeListStart + (int)i));
}

int MythDialog::CalcItemIndex(DialogCode code)
{
    return (int)code - (int)kDialogCodeListStart;
}

void MythDialog::accept()
{
    done(Accepted);
}

void MythDialog::reject()
{
    done(Rejected);
}

DialogCode MythDialog::exec(void)
{
    if (in_loop) 
    {
        qWarning("MythDialog::exec: Recursive call detected.");
        return kDialogCodeRejected;
    }

    setResult(kDialogCodeRejected);

    Show();

    in_loop = TRUE;

    QEventLoop *qteloop = QApplication::eventLoop();
    if (!qteloop)
        return kDialogCodeRejected;

    qteloop->enterLoop();

    DialogCode res = result();

    return res;
}

void MythDialog::hide(void)
{
    if (isHidden())
        return;

    // Reimplemented to exit a modal when the dialog is hidden.
    QWidget::hide();
    QEventLoop *qteloop = QApplication::eventLoop();
    if (in_loop && qteloop)
    {
        in_loop = FALSE;
        qteloop->exitLoop();
    }
}

void MythDialog::keyPressEvent( QKeyEvent *e )
{
    bool handled = false;
    QStringList actions;

    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE")
                reject();
            else if (action == "UP" || action == "LEFT")
            {
                if (focusWidget() &&
                    (focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                     focusWidget()->focusPolicy() == QWidget::WheelFocus))
                {
                }
                else
                    focusNextPrevChild(false);
            }
            else if (action == "DOWN" || action == "RIGHT")
            {
                if (focusWidget() &&
                    (focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                     focusWidget()->focusPolicy() == QWidget::WheelFocus)) 
                {
                }
                else
                    focusNextPrevChild(true);
            }
            else if (action == "MENU")
                emit menuButtonPressed();
            else
                handled = false;
        }
    }
}

/** \class MythPopupBox
 *  \brief Child of MythDialog used for most popup menus in MythTV
 *
 *  Most users of this class just call one of the static functions
 *  These create a dialog and block until it returns with a DialogCode.
 *
 *  When creating an instance yourself and using ExecPopup() or
 *  ShowPopup() you can optionally pass it a target and slot for
 *  the popupDone(int) signal. It will be sent with the DialogCode
 *  that the exec function returns, except it is cast to an int.
 *  This is most useful for ShowPopup() which doesn't block or
 *  return the result() when the popup is finished.
 */

MythPopupBox::MythPopupBox(MythMainWindow *parent, const char *name)
            : MythDialog(parent, name, false)
{
    float wmult, hmult;

    if (gContext->GetNumSetting("UseArrowAccels", 1))
        arrowAccel = true;
    else
        arrowAccel = false;

    gContext->GetScreenSettings(wmult, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setPalette(parent->palette());
    popupForegroundColor = foregroundColor ();
    setFont(parent->font());

    hpadding = gContext->GetNumSetting("PopupHeightPadding", 120);
    wpadding = gContext->GetNumSetting("PopupWidthPadding", 80);

    vbox = new QVBoxLayout(this, (int)(10 * hmult));
}

MythPopupBox::MythPopupBox(MythMainWindow *parent, bool graphicPopup,
                           QColor popupForeground, QColor popupBackground,
                           QColor popupHighlight, const char *name)
            : MythDialog(parent, name, false)
{
    float wmult, hmult;

    if (gContext->GetNumSetting("UseArrowAccels", 1))
        arrowAccel = true;
    else
        arrowAccel = false;

    gContext->GetScreenSettings(wmult, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setFrameStyle(QFrame::Box | QFrame::Plain);
    setPalette(parent->palette());
    setFont(parent->font());

    hpadding = gContext->GetNumSetting("PopupHeightPadding", 120);
    wpadding = gContext->GetNumSetting("PopupWidthPadding", 80);

    vbox = new QVBoxLayout(this, (int)(10 * hmult));

    if (!graphicPopup)
        setPaletteBackgroundColor(popupBackground);
    else
        gContext->ThemeWidget(this);
    setPaletteForegroundColor(popupHighlight);

    popupForegroundColor = popupForeground;
}

bool MythPopupBox::focusNextPrevChild(bool next)
{
    QFocusData *focusList = focusData();
    QObjectList *objList = queryList(NULL,NULL,false,true);

    QWidget *startingPoint = focusList->home();
    QWidget *candidate = NULL;

    QWidget *w = (next) ? focusList->prev() : focusList->next();

    int countdown = focusList->count();

    do
    {
        if (w && w != startingPoint && !w->focusProxy() && 
            w->isVisibleTo(this) && w->isEnabled() &&
            (objList->find((QObject *)w) != -1))
        {
            candidate = w;
        }

        w = (next) ? focusList->prev() : focusList->next();
    }   
    while (w && !(candidate && w == startingPoint) && (countdown-- > 0));

    if (!candidate)
        return false;

    candidate->setFocus();
    return true;
}

void MythPopupBox::addWidget(QWidget *widget, bool setAppearance)
{
    if (setAppearance == true)
    {
         widget->setPalette(palette());
         widget->setFont(font());
    }

    if (widget->isA("QLabel"))
    {
        widget->setBackgroundOrigin(ParentOrigin);
        widget->setPaletteForegroundColor(popupForegroundColor);
    }

    vbox->addWidget(widget);
}

QLabel *MythPopupBox::addLabel(QString caption, LabelSize size, bool wrap)
{
    QLabel *label = new QLabel(caption, this);
    switch (size)
    {
        case Large: label->setFont(defaultBigFont); break;
        case Medium: label->setFont(defaultMediumFont); break;
        case Small: label->setFont(defaultSmallFont); break;
    }
    
    label->setMaximumWidth((int)m_parent->width() / 2);
    if (wrap)
    {
        QChar::Direction text_dir = QChar::DirL;
        // Get a char from within the string to determine direction.
        if (caption.length())
            text_dir = caption[0].direction();
        int align = (QChar::DirAL == text_dir) ?
            Qt::WordBreak | Qt::AlignRight : Qt::WordBreak | Qt::AlignLeft;
        label->setAlignment(align);
    }

    addWidget(label, false);
    return label;
}

QButton *MythPopupBox::addButton(QString caption, QObject *target, 
                                 const char *slot)
{
    if (!target)
    {
        target = this;
        slot = SLOT(defaultButtonPressedHandler());
    }

    MythPushButton *button = new MythPushButton(caption, this, arrowAccel);
    m_parent->connect(button, SIGNAL(pressed()), target, slot);
    addWidget(button, false);
    return button;
}

void MythPopupBox::addLayout(QLayout *layout, int stretch)
{
    vbox->addLayout(layout, stretch);
}

void MythPopupBox::ShowPopup(QObject *target, const char *slot)
{
    ShowPopupAtXY(-1, -1, target, slot);
}

void MythPopupBox::ShowPopupAtXY(int destx, int desty, 
                                 QObject *target, const char *slot)
{
    const QObjectList *objlist = children();
    QObjectListIt it(*objlist);
    QObject *objs;

    while ((objs = it.current()) != 0)
    {
        ++it;
        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            widget->adjustSize();
        }
    }

    polish();

    int x = 0, y = 0, maxw = 0, poph = 0;

    it = QObjectListIt(*objlist);
    while ((objs = it.current()) != 0)
    {
        ++it;
        if (objs->isWidgetType())
        {
            QString objname = objs->name();
            if (objname != "nopopsize")
            {
                // little extra padding for these guys
                if (objs->isA("MythListBox"))
                    poph += (int)(25 * hmult);

                QWidget *widget = (QWidget *)objs;
                poph += widget->height();
                if (widget->width() > maxw)
                    maxw = widget->width();
            }
        }
    }

    poph += (int)(hpadding * hmult);
    setMinimumHeight(poph);

    maxw += (int)(wpadding * wmult);

    int width = (int)(800 * wmult);
    int height = (int)(600 * hmult);

    if (parentWidget())
    {
        width = parentWidget()->width();
        height = parentWidget()->height();
    }

    if (destx == -1)
        x = (int)(width / 2) - (int)(maxw / 2);
    else
        x = destx;

    if (desty == -1)
        y = (int)(height / 2) - (int)(poph / 2);
    else
        y = desty;

    if (poph + y > height)
        y = height - poph - (int)(8 * hmult);

    setFixedSize(maxw, poph);
    setGeometry(x, y, maxw, poph);

    if (target && slot)
        connect(this, SIGNAL(popupDone(int)), target, slot);

    Show();
}

void MythPopupBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions);
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];

        if ((action == "ESCAPE") || (arrowAccel && action == "LEFT"))
        {
            reject();
            handled = true;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythPopupBox::AcceptItem(int i)
{
    MythDialog::AcceptItem(i);
    emit popupDone(rescode);
}

void MythPopupBox::accept(void)
{
    MythDialog::done(MythDialog::Accepted);
    emit popupDone(MythDialog::Accepted);
}

void MythPopupBox::reject(void)
{
    MythDialog::done(MythDialog::Rejected);
    emit popupDone(MythDialog::Rejected);
}

DialogCode MythPopupBox::ExecPopup(QObject *target, const char *slot)
{
    if (!target)
        ShowPopup(this, SLOT(done(int)));
    else
        ShowPopup(target, slot);

    return exec();
}

DialogCode MythPopupBox::ExecPopupAtXY(int destx, int desty,
                                       QObject *target, const char *slot)
{
    if (!target)
        ShowPopupAtXY(destx, desty, this, SLOT(done(int)));
    else
        ShowPopupAtXY(destx, desty, target, slot);

    return exec();
}

void MythPopupBox::defaultButtonPressedHandler(void)
{
    const QObjectList *objlist = children();
    QObjectListIt itf(*objlist);
    QObject *objs;
    int i = 0;
    bool foundbutton = false;

    // this bit of code will work if the window is focused
    while ((objs = itf.current()) != 0)
    {
        ++itf;
        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            if (widget->isA("MythPushButton"))
            {
                if (widget->hasFocus())
                {
                    foundbutton = true;
                    break;
                }
                i++;
            }
        }
    }
    if (foundbutton)
    {
        AcceptItem(i);
        return;
    }

    // this bit of code should always work but requires a cast
    QObjectListIt itd(*objlist);
    i = 0;
    while ((objs = itd.current()) != 0)
    {
        ++itd;
        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            if (widget->isA("MythPushButton"))
            {
                MythPushButton *button = dynamic_cast<MythPushButton*>(widget);
                if (button && button->isDown())
                {
                    foundbutton = true;
                    break;
                }
                i++;
            }
        }
    }
    if (foundbutton)
    {
        AcceptItem(i);
        return;
    }

    VERBOSE(VB_IMPORTANT, "MythPopupBox::defaultButtonPressedHandler(void)"
            "\n\t\t\tWe should never get here!");
    done(kDialogCodeRejected);
}

bool MythPopupBox::showOkPopup(
    MythMainWindow *parent,
    const QString  &title,
    const QString  &message,
    QString         button_msg)
{
    if (button_msg.isEmpty())
        button_msg = QObject::tr("OK");

    MythPopupBox *popup = new MythPopupBox(parent, title);

    popup->addLabel(message, MythPopupBox::Medium, true);
    QButton *okButton = popup->addButton(button_msg, popup, SLOT(accept()));
    okButton->setFocus();
    bool ret = (kDialogCodeAccepted == popup->ExecPopup());

    popup->hide();
    popup->deleteLater();

    return ret;
}

bool MythPopupBox::showOkCancelPopup(MythMainWindow *parent, QString title,
                                     QString message, bool focusOk)
{
    MythPopupBox *popup = new MythPopupBox(parent, title);

    popup->addLabel(message, Medium, true);
    QButton *okButton = NULL, *cancelButton = NULL;
    okButton     = popup->addButton(tr("OK"),     popup, SLOT(accept()));
    cancelButton = popup->addButton(tr("Cancel"), popup, SLOT(reject()));

    if (focusOk)
        okButton->setFocus();
    else
        cancelButton->setFocus();

    bool ok = (Accepted == popup->ExecPopup());

    popup->hide();
    popup->deleteLater();

    return ok;
}

bool MythPopupBox::showGetTextPopup(MythMainWindow *parent, QString title,
                                    QString message, QString& text)
{
    MythPopupBox *popup = new MythPopupBox(parent, title);

    popup->addLabel(message, Medium, true);
    
    MythRemoteLineEdit *textEdit =
        new MythRemoteLineEdit(popup, "chooseEdit");

    textEdit->setText(text);
    popup->addWidget(textEdit);
    
    popup->addButton(tr("OK"),     popup, SLOT(accept()));
    popup->addButton(tr("Cancel"), popup, SLOT(reject()));
    
    textEdit->setFocus();
    
    bool ok = (Accepted == popup->ExecPopup());
    if (ok)
        text = QDeepCopy<QString>(textEdit->text());

    popup->hide();
    popup->deleteLater();

    return ok;
}

/**
 * \brief Like showGetTextPopup(), but doesn't echo the text entered.
 *
 * Uses MythLineEdit instead of MythRemoteLineEdit, so that PINs can be
 * entered without having to cycle through the number key "phone entry" mode.
 */
QString MythPopupBox::showPasswordPopup(MythMainWindow *parent,
                                        QString        title,
                                        QString        message)
{
    MythPopupBox *popup = new MythPopupBox(parent, title);

    popup->addLabel(message, Medium, true);

    MythLineEdit *entry = new MythLineEdit(popup, "passwordEntry");
    entry->setEchoMode(QLineEdit::Password);

    popup->addWidget(entry);

    popup->addButton(tr("OK"),    popup, SLOT(accept()));
    popup->addButton(tr("Cancel"),popup, SLOT(reject()));

    // Currently unused, because AllowVirtualKeyboard is set
    popup->m_parent->connect(entry, SIGNAL(returnPressed()),
                             popup, SLOT(accept()));
    entry->setFocus();

    QString password = QString::null;
    if (popup->ExecPopup() == Accepted)
        password = QDeepCopy<QString>(entry->text());

    popup->hide();
    popup->deleteLater();

    return password;
}


DialogCode MythPopupBox::ShowButtonPopup(
    MythMainWindow    *parent,
    const QString     &title,
    const QString     &message,
    const QStringList &buttonmsgs,
    DialogCode         default_button)
{
    MythPopupBox *popup = new MythPopupBox(parent, title);

    popup->addLabel(message, Medium, true);
    popup->addLabel("");

    const uint def = CalcItemIndex(default_button);
    for (unsigned int i = 0; i < buttonmsgs.size(); i++ )
    {
        QButton *but = popup->addButton(buttonmsgs[i]);
        if (def == i)
            but->setFocus();
    }

    DialogCode ret = popup->ExecPopup();

    popup->hide();
    popup->deleteLater();

    return ret;
}

MythProgressDialog::MythProgressDialog(const QString &message, int totalSteps, 
                                         bool cancelButton, const QObject *target, const char *slot)
                  : MythDialog(gContext->GetMainWindow(), "progress", false)
{
    int screenwidth, screenheight;
    float wmult, hmult;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setFont(gContext->GetMediumFont());

    gContext->ThemeWidget(this);

    int yoff = screenheight / 3;
    int xoff = screenwidth / 10;
    setGeometry(xoff, yoff, screenwidth - xoff * 2, yoff);
    setFixedSize(QSize(screenwidth - xoff * 2, yoff));

    QVBoxLayout *lay = new QVBoxLayout(this, 0);

    QVBox *vbox = new QVBox(this);
    lay->addWidget(vbox);

    vbox->setLineWidth(3);
    vbox->setMidLineWidth(3);
    vbox->setFrameShape(QFrame::Panel);
    vbox->setFrameShadow(QFrame::Raised);
    vbox->setMargin((int)(15 * wmult));

    msglabel = new QLabel(vbox);
    msglabel->setBackgroundOrigin(ParentOrigin);
    msglabel->setText(message);
    vbox->setStretchFactor(msglabel, 5);

    QHBox *hbox = new QHBox(vbox);
    hbox->setSpacing(5);
    
    progress = new QProgressBar(totalSteps, hbox);
    progress->setBackgroundOrigin(ParentOrigin);

    if (cancelButton && slot && target)
    {
        MythPushButton *button = new MythPushButton("Cancel", hbox, 0);
        button->setFocus();
        connect(button, SIGNAL(pressed()), target, slot);
    }

    setTotalSteps(totalSteps);

    if (class LCD * lcddev = LCD::Get())
    {
        textItems = new QPtrList<LCDTextItem>;
        textItems->setAutoDelete(true);

        textItems->clear();
        textItems->append(new LCDTextItem(1, ALIGN_CENTERED, message, "Generic",
                          false));
        lcddev->switchToGeneric(textItems);
    }
    else 
        textItems = NULL;

    show();

    qApp->processEvents();
}

MythProgressDialog::~MythProgressDialog()
{
    Teardown();
}

void MythProgressDialog::deleteLater(void)
{
    hide();
    Teardown();
    MythDialog::deleteLater();
}

void MythProgressDialog::Teardown(void)
{
    if (textItems)
    {
        delete textItems;
        textItems = NULL;
    }
}

void MythProgressDialog::Close(void)
{
    accept();

    LCD *lcddev = LCD::Get();
    if (lcddev)
    {
        lcddev->switchToNothing();
        lcddev->switchToTime();
    }
}

void MythProgressDialog::setProgress(int curprogress)
{
    progress->setProgress(curprogress);
    if (curprogress % steps == 0)
    {
        qApp->processEvents();
        if (LCD * lcddev = LCD::Get())
        {
            float fProgress = (float)curprogress / m_totalSteps;
            lcddev->setGenericProgress(fProgress);
        }
    }
}

void MythProgressDialog::setLabel(QString newlabel)
{
    msglabel->setText(newlabel);
}

void MythProgressDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
                handled = true;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythProgressDialog::setTotalSteps(int totalSteps)
{
    m_totalSteps = totalSteps;
    progress->setTotalSteps(totalSteps);
    steps = totalSteps / 1000;
    if (steps == 0)
        steps = 1;
}

MythBusyDialog::MythBusyDialog(const QString &title,
                               bool cancelButton, const QObject *target, const char *slot)
    : MythProgressDialog(title, 0,
                         cancelButton, target, slot),
                         timer(NULL)
{
}

MythBusyDialog::~MythBusyDialog()
{
    Teardown();
}

void MythBusyDialog::deleteLater(void)
{
    Teardown();
    MythProgressDialog::deleteLater();
}

void MythBusyDialog::Teardown(void)
{
    if (timer)
    {
        timer->disconnect();
        timer->deleteLater();
        timer = NULL;
    }
}

void MythBusyDialog::start(int interval)
{
    if (!timer)
        timer = new QTimer (this);

    connect(timer, SIGNAL(timeout()),
            this,  SLOT  (timeout()));

    timer->start(interval);
}

void MythBusyDialog::Close(void)
{
    if (timer)
    {
        timer->disconnect();
        timer->deleteLater();
        timer = NULL;
    }

    MythProgressDialog::Close();
}

void MythBusyDialog::setProgress(void)
{
    progress->setProgress(progress->progress () + 10);
    qApp->processEvents();
    if (LCD *lcddev = LCD::Get())
        lcddev->setGenericBusy();
}

void MythBusyDialog::timeout(void)
{
    setProgress();
}

MythThemedDialog::MythThemedDialog(MythMainWindow *parent, QString window_name,
                                   QString theme_filename, const char* name,
                                   bool setsize)
                : MythDialog(parent, name, setsize)
{
    setNoErase();

    theme = NULL;

    if (!loadThemedWindow(window_name, theme_filename))
    {
        QString msg = 
            QString(tr("Could not locate '%1' in theme '%2'."
                       "\n\nReturning to the previous menu."))
            .arg(window_name).arg(theme_filename);
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("Missing UI Element"), msg);
        reject();
        return;
    }
}

MythThemedDialog::MythThemedDialog(MythMainWindow *parent, const char* name,
                                   bool setsize)
                : MythDialog(parent, name, setsize)
{
    setNoErase();
    theme = NULL;
}

bool MythThemedDialog::loadThemedWindow(QString window_name, QString theme_filename)
{

    if (theme)
        delete theme;

    context = -1;
    my_containers.clear();
    widget_with_current_focus = NULL;

    redrawRect = QRect(0, 0, 0, 0);

    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if (!theme->LoadTheme(xmldata, window_name, theme_filename))
    {
        return false;
    }

    loadWindow(xmldata);

    //
    //  Auto-connect signals we know about
    //

    //  Loop over containers
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;
    while ((looper = an_it.current()) != 0)
    {
        //  Loop over UITypes within each container
        vector<UIType *> *all_ui_type_objects = looper->getAllTypes();
        vector<UIType *>::iterator i = all_ui_type_objects->begin();
        for (; i != all_ui_type_objects->end(); i++)
        {
            UIType *type = (*i);
            connect(type, SIGNAL(requestUpdate()), this, 
                    SLOT(updateForeground()));
            connect(type, SIGNAL(requestUpdate(const QRect &)), this, 
                    SLOT(updateForeground(const QRect &)));
            connect(type, SIGNAL(requestRegionUpdate(const QRect &)), this,
                    SLOT(updateForegroundRegion(const QRect &)));
        }
        ++an_it;
    }

    buildFocusList();

    updateBackground();
    initForeground();

    return true;
}

bool MythThemedDialog::buildFocusList()
{
    //
    //  Build a list of widgets that will take focus
    //

    focus_taking_widgets.clear();


    //  Loop over containers
    LayerSet *looper;
    QPtrListIterator<LayerSet> another_it(my_containers);
    while ((looper = another_it.current()) != 0)
    {
        //  Loop over UITypes within each container
        vector<UIType *> *all_ui_type_objects = looper->getAllTypes();
        vector<UIType *>::iterator i = all_ui_type_objects->begin();
        for (; i != all_ui_type_objects->end(); i++)
        {
            UIType *type = (*i);
            if (type->canTakeFocus() && !type->isHidden())
            {
                if (context == -1 || type->GetContext() == -1 || 
                        context == type->GetContext())
                    focus_taking_widgets.append(type);
            }
        }
        ++another_it;
    }
    if (focus_taking_widgets.count() > 0)
    {
        return true;
    }
    return false;
}

MythThemedDialog::~MythThemedDialog()
{
    if (theme)
    {
        delete theme;
        theme = NULL;
    }
}

void MythThemedDialog::deleteLater(void)
{
    if (theme)
    {
        delete theme;
        theme = NULL;
    }
    MythDialog::deleteLater();
}

void MythThemedDialog::loadWindow(QDomElement &element)
{
    //
    //  Parse all the child elements in the theme
    //

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else if (e.tagName() == "popup")
            {
                parsePopup(e);
            }
            else
            {
                VERBOSE(VB_IMPORTANT,
                        QString("MythThemedDialog::loadWindow(): Do not "
                                "understand DOM Element: '%1'. Ignoring.")
                        .arg(e.tagName()));
            }
        }
    }
}

void MythThemedDialog::parseContainer(QDomElement &element)
{
    //
    //  Have the them object parse the containers
    //  but hold a pointer to each of them so
    //  that we can iterate over them later
    //

    QRect area;
    QString name;
    int a_context;
    theme->parseContainer(element, name, a_context, area);
    if (name.length() < 1)
    {
        VERBOSE(VB_IMPORTANT, "Failed to parse a container. Ignoring.");
        return;
    }

    LayerSet *container_reference = theme->GetSet(name);
    my_containers.append(container_reference);
}

void MythThemedDialog::parseFont(QDomElement &element)
{
    //
    //  this is just here so you can re-implement the virtual
    //  function and do something special if you like
    //

    theme->parseFont(element);
}

void MythThemedDialog::parsePopup(QDomElement &element)
{
    //
    //  theme doesn't know how to do this yet
    //
    element = element;
    cerr << "I don't know how to handle popops yet (I'm going to try and "
            "just ignore it)\n";
}

void MythThemedDialog::initForeground()
{
    my_foreground = my_background;
    updateForeground();
}

void MythThemedDialog::updateBackground()
{
    //
    //  Draw the background pixmap once
    //

    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    //
    //  Ask the container holding anything
    //  to do with the background to draw
    //  itself on a pixmap
    //

    LayerSet *container = theme->GetSet("background");

    //
    //  *IFF* there is a background, draw it
    //
    if (container)
    {
        container->Draw(&tmp, 0, context);
        tmp.end();
    }

    //
    //  Copy that pixmap to the permanent one
    //  and tell Qt about it
    //

    my_background = bground;
    setPaletteBackgroundPixmap(my_background);
}

void MythThemedDialog::updateForeground()
{
    QRect r = this->geometry();
    updateForeground(r);
}

void MythThemedDialog::updateForeground(const QRect &r)
{
    QRect rect_to_update = r;
    if (r.width() == 0 || r.height() == 0)
    {
        cerr << "MythThemedDialog.o: something is requesting a screen update of zero size. "
             << "A widget probably has not done a calculateScreeArea(). Will redraw "
             << "the whole screen (inefficient!)." 
             << endl;
             
        rect_to_update = this->geometry();
    }

    redrawRect = redrawRect.unite(r);

    update(redrawRect);
}

void MythThemedDialog::ReallyUpdateForeground(const QRect &r)
{
    QRect rect_to_update = r;
    if (r.width() == 0 || r.height() == 0)
    {
        cerr << "MythThemedDialog.o: something is requesting a screen update of zero size. "
             << "A widget probably has not done a calculateScreeArea(). Will redraw "
             << "the whole screen (inefficient!)."
             << endl;

        rect_to_update = this->geometry();
    }

    UpdateForegroundRect(rect_to_update);

    redrawRect = QRect(0, 0, 0, 0);
}

void MythThemedDialog::updateForegroundRegion(const QRect &r)
{
    // Note: DrawRegion is never actually called now. Instead
    // of controls (only UIListTreeType did it) implementing an
    // optimized paint, we rely on clipping regions.
    UpdateForegroundRect(r);

    update(r);
}

void MythThemedDialog::UpdateForegroundRect(const QRect &inv_rect)
{
    QPainter whole_dialog_painter(&my_foreground);

    // Updating the background portion isn't optional. The old
    // behavior left remnants during context transitions if
    // they happened outside active containers for the current
    // context.
    whole_dialog_painter.drawPixmap(inv_rect.topLeft(), my_background,
                                    inv_rect);

    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while ((looper = an_it.current()) != 0)
    {
        QRect container_area = looper->GetAreaRect();

        //
        //  Only paint if the container's area is valid
        //  and it intersects with whatever Qt told us
        //  needed to be repainted
        //

        const QRect intersect = inv_rect.intersect(container_area);
        int looper_context = looper->GetContext();
        if (container_area.isValid() &&
            (looper_context == context || looper_context == -1) &&
            intersect.isValid() &&
            looper->GetName().lower() != "background")
        {
            //
            //  Debugging
            //
            /*
            cout << "A container called \"" << looper->GetName()
                 << "\" said its area is "
                 << container_area.left()
                 << ","
                 << container_area.top()
                 << " to "
                 << container_area.left() + container_area.width()
                 << ","
                 << container_area.top() + container_area.height()
                 << endl;
            */

            //
            //  Loop over the draworder layers

            whole_dialog_painter.save();

            whole_dialog_painter.setClipRect(intersect);
            whole_dialog_painter.translate(container_area.left(),
                                           container_area.top());

            for (int i = 0; i <= looper->getLayers(); ++i)
            {
                looper->Draw(&whole_dialog_painter, i, context);
            }

            whole_dialog_painter.restore();
        }
        ++an_it;
    }
}

void MythThemedDialog::paintEvent(QPaintEvent *e)
{
    if (redrawRect.width() > 0 && redrawRect.height() > 0)
        ReallyUpdateForeground(redrawRect);

    bitBlt(this, e->rect().left(), e->rect().top(),
           &my_foreground, e->rect().left(), e->rect().top(),
           e->rect().width(), e->rect().height());

    MythDialog::paintEvent(e);
}

bool MythThemedDialog::assignFirstFocus()
{
    if (widget_with_current_focus)
    {
        widget_with_current_focus->looseFocus();
    }

    QPtrListIterator<UIType> an_it(focus_taking_widgets);
    UIType *looper;

    while ((looper = an_it.current()) != 0)
    {
        if (looper->canTakeFocus())
        {
            widget_with_current_focus = looper;
            widget_with_current_focus->takeFocus();
            return true;
        }
        ++an_it;
    }

    return false;
}

bool MythThemedDialog::nextPrevWidgetFocus(bool up_or_down)
{
    if (up_or_down)
    {
        bool reached_current = false;
        QPtrListIterator<UIType> an_it(focus_taking_widgets);
        UIType *looper;

        while ((looper = an_it.current()) != 0)
        {
            if (reached_current && looper->canTakeFocus())
            {
                widget_with_current_focus->looseFocus();
                widget_with_current_focus = looper;
                widget_with_current_focus->takeFocus();
                return true;
            }

            if (looper == widget_with_current_focus)
            {
                reached_current= true;
            }
            ++an_it;
        }

        if (assignFirstFocus())
        {
            return true;
        }
        return false;
    }
    else
    {
        bool reached_current = false;
        QPtrListIterator<UIType> an_it(focus_taking_widgets);
        an_it.toLast();
        UIType *looper;

        while ((looper = an_it.current()) != 0)
        {
            if (reached_current && looper->canTakeFocus())
            {
                widget_with_current_focus->looseFocus();
                widget_with_current_focus = looper;
                widget_with_current_focus->takeFocus();
                return true;
            }

            if (looper == widget_with_current_focus)
            {
                reached_current= true;
            }
            --an_it;
        }

        if (reached_current)
        {
            an_it.toLast();
            while ((looper = an_it.current()) != 0)
            {
                if (looper->canTakeFocus())
                {
                    widget_with_current_focus->looseFocus();
                    widget_with_current_focus = looper;
                    widget_with_current_focus->takeFocus();
                    return true;
                }
                --an_it;
            }
        }
        return false;
    }
    return false;
}

void MythThemedDialog::activateCurrent()
{
    if (widget_with_current_focus)
    {
        widget_with_current_focus->activate();
    }
    else
    {
        cerr << "dialogbox.o: Something asked me activate the current widget, "
                "but there is no current widget\n";
    }
}

namespace
{
    template <typename T>
    T *GetUIType(MythThemedDialog *dialog, const QString &name)
    {
        UIType *sf = dialog->getUIObject(name);
        if (sf)
        {
            T *ret = dynamic_cast<T *>(sf);
            if (ret)
                return ret;
        }
        return 0;
    }
};

UIType* MythThemedDialog::getUIObject(const QString &name)
{
    //
    //  Try and find the UIType target referenced
    //  by "name".
    //
    //  At some point, it would be nice to speed
    //  this up with a map across all instantiated
    //  UIType objects "owned" by this dialog
    //

    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while ((looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
            return hunter;
        ++an_it;
    }

    return NULL;
}

UIType* MythThemedDialog::getCurrentFocusWidget()
{
    if (widget_with_current_focus)
    {
        return widget_with_current_focus;
    }
    return NULL;
}

void MythThemedDialog::setCurrentFocusWidget(UIType* widget)
{
    // make sure this widget is in the list of widgets that can take focus
    if (focus_taking_widgets.find(widget) == -1)
        return;

    if (widget_with_current_focus)
        widget_with_current_focus->looseFocus();

    widget_with_current_focus = widget;
    widget_with_current_focus->takeFocus();
}

UIManagedTreeListType* MythThemedDialog::getUIManagedTreeListType(const QString &name)
{
    return GetUIType<UIManagedTreeListType>(this, name);
}

UITextType* MythThemedDialog::getUITextType(const QString &name)
{
    return GetUIType<UITextType>(this, name);
}

UIRichTextType* MythThemedDialog::getUIRichTextType(const QString &name)
{
    return GetUIType<UIRichTextType>(this, name);
}

UIRemoteEditType* MythThemedDialog::getUIRemoteEditType(const QString &name)
{
    return GetUIType<UIRemoteEditType>(this, name);
}

UIMultiTextType* MythThemedDialog::getUIMultiTextType(const QString &name)
{
    return GetUIType<UIMultiTextType>(this, name);
}

UIPushButtonType* MythThemedDialog::getUIPushButtonType(const QString &name)
{
    return GetUIType<UIPushButtonType>(this, name);
}

UITextButtonType* MythThemedDialog::getUITextButtonType(const QString &name)
{
    return GetUIType<UITextButtonType>(this, name);
}

UIRepeatedImageType* MythThemedDialog::getUIRepeatedImageType(const QString &name)
{
    return GetUIType<UIRepeatedImageType>(this, name);
}

UICheckBoxType* MythThemedDialog::getUICheckBoxType(const QString &name)
{
    return GetUIType<UICheckBoxType>(this, name);
}

UISelectorType* MythThemedDialog::getUISelectorType(const QString &name)
{
    return GetUIType<UISelectorType>(this, name);
}

UIBlackHoleType* MythThemedDialog::getUIBlackHoleType(const QString &name)
{
    return GetUIType<UIBlackHoleType>(this, name);
}

UIImageType* MythThemedDialog::getUIImageType(const QString &name)
{
    return GetUIType<UIImageType>(this, name);
}

UIStatusBarType* MythThemedDialog::getUIStatusBarType(const QString &name)
{
    return GetUIType<UIStatusBarType>(this, name);
}

UIListBtnType* MythThemedDialog::getUIListBtnType(const QString &name)
{
    return GetUIType<UIListBtnType>(this, name);
}

UIListTreeType* MythThemedDialog::getUIListTreeType(const QString &name)
{
    return GetUIType<UIListTreeType>(this, name);
}

UIKeyboardType *MythThemedDialog::getUIKeyboardType(const QString &name)
{
    return GetUIType<UIKeyboardType>(this, name);
}

UIImageGridType* MythThemedDialog::getUIImageGridType(const QString &name)
{
    return GetUIType<UIImageGridType>(this, name);
}

LayerSet* MythThemedDialog::getContainer(const QString& name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        if (looper->GetName() == name)
            return looper;
        
        ++an_it;
    }
    
    return NULL;
}

fontProp* MythThemedDialog::getFont(const QString &name)
{
    fontProp* font = NULL;
    if (theme)
        font = theme->GetFont(name, true);

    return font;
}

/**
 * \class MythPasswordDialog
 * \brief Display a window which accepts a password and verifies it.
 *
 * Needs the password to be passed in as an argument, because it does the
 * checking locally, and sets a bool if the entered password matches it.
 */

/**
 * \param success Pointer to storage for the result
 * \param target  The password we are trying to match
 */
MythPasswordDialog::MythPasswordDialog(QString message,
                                       bool *success,
                                       QString target,
                                       MythMainWindow *parent, 
                                       const char *name, 
                                       bool)
                   :MythDialog(parent, name, false)
{
    int textWidth =  fontMetrics().width(message);
    int totalWidth = textWidth + 175;

    success_flag = success;
    target_text = target;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    this->setGeometry((screenwidth - 250 ) / 2,
                      (screenheight - 50 ) / 2,
                      totalWidth,50);
    QFrame *outside_border = new QFrame(this);
    outside_border->setGeometry(0,0,totalWidth,50);
    outside_border->setFrameStyle(QFrame::Panel | QFrame::Raised );
    outside_border->setLineWidth(4);
    QLabel *message_label = new QLabel(message, this);
    message_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    message_label->setGeometry(15,10,textWidth,30);
    message_label->setBackgroundOrigin(ParentOrigin);
    password_editor = new MythLineEdit(this);
    password_editor->setEchoMode(QLineEdit::Password);
    password_editor->setGeometry(textWidth + 20,10,135,30);
    password_editor->setBackgroundOrigin(ParentOrigin);
    password_editor->setAllowVirtualKeyboard(false);
    connect(password_editor, SIGNAL(textChanged(const QString &)),
            this, SLOT(checkPassword(const QString &)));

    this->setActiveWindow();
    password_editor->setFocus();
}

void MythPasswordDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                handled = true;
                MythDialog::keyPressEvent(e);
            }
        }
    }
}


void MythPasswordDialog::checkPassword(const QString &the_text)
{
    if (the_text == target_text)
    {
        *success_flag = true;
        accept();
    }
    else
    {
        //  Oh to beep 
    }
}

MythPasswordDialog::~MythPasswordDialog()
{
}

/*
---------------------------------------------------------------------
*/

MythSearchDialog::MythSearchDialog(MythMainWindow *parent, const char *name) 
                 :MythPopupBox(parent, name)
{
    // create the widgets
    caption = addLabel(QString(""));

    editor = new MythRemoteLineEdit(this);
    connect(editor, SIGNAL(textChanged()), this, SLOT(searchTextChanged()));
    addWidget(editor);
    editor->setFocus();
    editor->setPopupPosition(VK_POSBOTTOMDIALOG); 

    listbox = new MythListBox(this);
    listbox->setScrollBar(false);
    listbox->setBottomScrollBar(false);
    connect(listbox, SIGNAL(accepted(int)), this, SLOT(AcceptItem(int)));
    addWidget(listbox);

    ok_button     = addButton(tr("OK"),     this, SLOT(accept()));
    cancel_button = addButton(tr("Cancel"), this, SLOT(reject()));
}

void MythSearchDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
            {
                handled = true;
                reject();
            }
            if (action == "LEFT")
            {
                handled = true;
                focusNextPrevChild(false);
            }
            if (action == "RIGHT")
            {
                handled = true;
                focusNextPrevChild(true);
            }
            if (action == "SELECT")
            {
                handled = true;
                accept();
            }
        }
    }
    if (!handled)
        MythPopupBox::keyPressEvent(e);
}

void MythSearchDialog::setCaption(QString text)
{
    if (caption)
        caption->setText(text);
}

void MythSearchDialog::setSearchText(QString text)
{
    if (editor)
    {
        editor->setText(text);
        editor->setCursorPosition(0, editor->text().length());
    }
}

void MythSearchDialog::searchTextChanged(void)
{
    if (listbox && editor)
    {
        listbox->setCurrentItem(editor->text(), false,  true);
        listbox->setTopItem(listbox->currentItem());
    }
}

QString MythSearchDialog::getResult(void)
{
    if (listbox)
        return listbox->currentText();

    // Don't return QString::null, might cause segfaults due to
    // code that doesn't check the return value...
    return "";
}

void MythSearchDialog::setItems(QStringList items)
{
    if (listbox)
    {
        listbox->insertStringList(items);
        searchTextChanged();
    }
}

MythSearchDialog::~MythSearchDialog()
{
    Teardown();
}

void MythSearchDialog::deleteLater(void)
{
    Teardown();
    MythPopupBox::deleteLater();
}

void MythSearchDialog::Teardown(void)
{
    caption       = NULL; // deleted by Qt

    if (editor)
    {
        editor->disconnect();
        editor    = NULL; // deleted by Qt
    }

    if (listbox)
    {
        listbox->disconnect();
        listbox   = NULL; // deleted by Qt
    }

    ok_button     = NULL; // deleted by Qt
    cancel_button = NULL; // deleted by Qt
}

/*
---------------------------------------------------------------------
 */

/** \fn MythImageFileDialog::MythImageFileDialog(QString*,QString,MythMainWindow*,QString,QString,const char*,bool)
 *  \brief Locates an image file dialog in the current theme.
 *
 *   MythImageFileDialog's expect there to be certain
 *   elements in the theme, or they will fail.
 *
 *   For example, we use the size of the background
 *   pixmap to define the geometry of the dialog. If
 *   the pixmap ain't there, we need to fail.
 */
MythImageFileDialog::MythImageFileDialog(QString *result,
                                         QString top_directory,
                                         MythMainWindow *parent, 
                                         QString window_name,
                                         QString theme_filename,
                                         const char *name, 
                                         bool setsize)
                   :MythThemedDialog(parent, 
                                     window_name, 
                                     theme_filename,
                                     name,
                                     setsize)
{
    selected_file = result;
    initial_node = NULL;
    
    UIImageType *file_browser_background = getUIImageType("file_browser_background");
    if (file_browser_background)
    {
        QPixmap background = file_browser_background->GetImage();
        
        this->setFixedSize(QSize(background.width(), background.height()));
        this->move((screenwidth - background.width()) / 2,
                   (screenheight - background.height()) / 2);
    }
    else
    {
        QString msg = 
            QString(tr("The theme you are using is missing the "
                       "'file_browser_background' "
                       "element. \n\nReturning to the previous menu."));
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("Missing UI Element"), msg);
        reject();
        return;
    }

    //
    //  Make a nice border
    //

    this->setFrameStyle(QFrame::Panel | QFrame::Raised );
    this->setLineWidth(4);


    //
    //  Find the managed tree list that handles browsing.
    //  Fail if we can't find it.
    //

    file_browser = getUIManagedTreeListType("file_browser");
    if (file_browser)
    {
        file_browser->calculateScreenArea();
        file_browser->showWholeTree(true);
        connect(file_browser, SIGNAL(nodeSelected(int, IntVector*)),
                this, SLOT(handleTreeListSelection(int, IntVector*)));
        connect(file_browser, SIGNAL(nodeEntered(int, IntVector*)),
                this, SLOT(handleTreeListEntered(int, IntVector*)));
    }
    else
    { 
        QString msg = 
            QString(tr("The theme you are using is missing the "
                       "'file_browser' element. "
                       "\n\nReturning to the previous menu."));
        MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                  tr("Missing UI Element"), msg);
        reject();
        return;
    }    
    
    //
    //  Find the UIImageType for previewing
    //  No image_box, no preview.
    //
    
    image_box = getUIImageType("image_box");
    if (image_box)
    {
        image_box->calculateScreenArea();
    }
    initialDir = top_directory;
        
    image_files.clear();
    buildTree(top_directory);

    file_browser->assignTreeData(root_parent);
    if (initial_node)
    {
        file_browser->setCurrentNode(initial_node);
    }
    file_browser->enter();
    file_browser->refresh();
    
}

void MythImageFileDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                file_browser->moveUp();
            else if (action == "DOWN")
                file_browser->moveDown();
            else if (action == "LEFT")
                file_browser->popUp();
            else if (action == "RIGHT")
                file_browser->pushDown();
            else if (action == "PAGEUP")
                file_browser->pageUp();
            else if (action == "PAGEDOWN")
                file_browser->pageDown();
            else if (action == "SELECT")
                file_browser->select();
            else
                handled = false;
        }
    }

    if (!handled)
        MythThemedDialog::keyPressEvent(e);
}

void MythImageFileDialog::buildTree(QString starting_where)
{
    buildFileList(starting_where);
    
    root_parent = new GenericTree("Image Files root", -1, false);
    file_root = root_parent->addNode("Image Files", -1, false);

    //
    //  Go through the files and build a tree
    //    
    for(uint i = 0; i < image_files.count(); ++i)
    {
        bool make_active = false;
        QString file_string = *(image_files.at(i));
        if (file_string == *selected_file)
        {
            make_active = true;
        }
        QString prefix = initialDir;

        if (prefix.length() < 1)
        {
            cerr << "mythdialogs.o: Seems unlikely that this is going to work" << endl;
        }
        file_string.remove(0, prefix.length());
        QStringList list(QStringList::split("/", file_string));
        GenericTree *where_to_add;
        where_to_add = file_root;
        int a_counter = 0;
        QStringList::Iterator an_it = list.begin();
        for( ; an_it != list.end(); ++an_it)
        {
            if (a_counter + 1 >= (int) list.count())
            {
                QString title = (*an_it);
                GenericTree *added_node = where_to_add->addNode(title.section(".",0,0), i, true);
                if (make_active)
                {
                    initial_node = added_node;
                }
            }
            else
            {
                QString dirname = *an_it + "/";
                GenericTree *sub_node;
                sub_node = where_to_add->getChildByName(dirname);
                if (!sub_node)
                {
                    sub_node = where_to_add->addNode(dirname, -1, false);
                }
                where_to_add = sub_node;
            }
            ++a_counter;
        }
    }
    if (file_root->childCount() < 1)
    {
        //
        //  Nothing survived the requirements
        //
        file_root->addNode("No files found", -1, false);
    }
}

void MythImageFileDialog::buildFileList(QString directory)
{
    QStringList imageExtensions = QImage::inputFormatList();

    // Expand the list Qt gives us, working off what we know was built into
    // Qt based on the list it gave us

    if (imageExtensions.contains("jpeg") || imageExtensions.contains("JPEG"))
        imageExtensions += "jpg";

    QDir d(directory);
       
    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();

    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;
    QRegExp r;
    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." ||
            fi->fileName() == ".." )
        {
            continue;
        }
            
        if (fi->isDir())
        {
            buildFileList(fi->absFilePath());
        }
        else
        {
            r.setPattern("^" + fi->extension() + "$");
            r.setCaseSensitive(false);
            QStringList result = imageExtensions.grep(r);
            if (!result.isEmpty())
            {
                image_files.append(fi->absFilePath());
            }
            else
            {
                r.setPattern("^" + fi->extension());
                r.setCaseSensitive(false);
                QStringList other_result = imageExtensions.grep(r);
                if (!result.isEmpty())
                {
                    image_files.append(fi->absFilePath());
                }
            }
        }
    }                                                                                                                                                                                                                                                               
}

void MythImageFileDialog::handleTreeListEntered(int type, IntVector*)
{
    if (image_box)
    {
        if (type > -1)
        {
            image_box->SetImage(image_files[type]);
        }
        else
        {
            image_box->SetImage("");
        }
        image_box->LoadImage();
    }
}

void MythImageFileDialog::handleTreeListSelection(int type, IntVector*)
{
    if (type > -1)
    {
        *selected_file = image_files[type];
        accept();
    }   
}

MythImageFileDialog::~MythImageFileDialog()
{
    if (root_parent)
    {
        root_parent->deleteAllChildren();
        delete root_parent;
        root_parent = NULL;
    }
}

// -----------------------------------------------------------------------------

MythScrollDialog::MythScrollDialog(MythMainWindow *parent,
                                   MythScrollDialog::ScrollMode mode,
                                   const char *name)
    : QScrollView(parent, name)
{
    if (!parent)
    {
        VERBOSE(VB_IMPORTANT, 
                "MythScrollDialog: Programmer error, trying to create "
                "a dialog without a parent.");
        done(kDialogCodeRejected);
        return;
    }

    m_parent     = parent;
    m_scrollMode = mode;
        
    m_resCode    = kDialogCodeRejected;
    m_inLoop     = false;
    
    gContext->GetScreenSettings(m_xbase, m_screenWidth, m_wmult,
                                m_ybase, m_screenHeight, m_hmult);

    m_defaultBigFont = gContext->GetBigFont();
    m_defaultMediumFont = gContext->GetMediumFont();
    m_defaultSmallFont = gContext->GetSmallFont();

    setFont(m_defaultMediumFont);
    setCursor(QCursor(Qt::ArrowCursor));
    
    setFrameShape(QFrame::NoFrame);
    setHScrollBarMode(QScrollView::AlwaysOff);
    setVScrollBarMode(QScrollView::AlwaysOff);
    setFixedSize(QSize(m_screenWidth, m_screenHeight));

    gContext->ThemeWidget(viewport());
    if (viewport()->paletteBackgroundPixmap())
        m_bgPixmap = new QPixmap(*(viewport()->paletteBackgroundPixmap()));
    else {
        m_bgPixmap = new QPixmap(m_screenWidth, m_screenHeight);
        m_bgPixmap->fill(viewport()->colorGroup().base());
    }
    viewport()->setBackgroundMode(Qt::NoBackground);

    m_upArrowPix = gContext->LoadScalePixmap("scrollarrow-up.png");
    m_dnArrowPix = gContext->LoadScalePixmap("scrollarrow-dn.png");
    m_ltArrowPix = gContext->LoadScalePixmap("scrollarrow-left.png");
    m_rtArrowPix = gContext->LoadScalePixmap("scrollarrow-right.png");

    int wmargin = (int)(20*m_wmult);
    int hmargin = (int)(20*m_hmult);
    
    if (m_upArrowPix)
        m_upArrowRect = QRect(m_screenWidth - m_upArrowPix->width() - wmargin,
                              hmargin,
                              m_upArrowPix->width(), m_upArrowPix->height());
    if (m_dnArrowPix)
        m_dnArrowRect = QRect(m_screenWidth - m_dnArrowPix->width() - wmargin,
                              m_screenHeight - m_dnArrowPix->height() - hmargin,
                              m_dnArrowPix->width(), m_dnArrowPix->height());
    if (m_rtArrowPix)
        m_rtArrowRect = QRect(m_screenWidth - m_rtArrowPix->width() - wmargin,
                              m_screenHeight - m_rtArrowPix->height() - hmargin,
                              m_rtArrowPix->width(), m_rtArrowPix->height());
    if (m_ltArrowPix)
        m_ltArrowRect = QRect(wmargin,
                              m_screenHeight - m_ltArrowPix->height() - hmargin,
                              m_ltArrowPix->width(), m_ltArrowPix->height());
    
    m_showUpArrow  = true;
    m_showDnArrow  = true;
    m_showRtArrow  = false;
    m_showLtArrow  = false;
    
    m_parent->attach(this);
}

MythScrollDialog::~MythScrollDialog()
{
    m_parent->detach(this);
    delete m_bgPixmap;

    if (m_upArrowPix)
        delete m_upArrowPix;
    if (m_dnArrowPix)
        delete m_dnArrowPix;
    if (m_ltArrowPix)
        delete m_ltArrowPix;
    if (m_rtArrowPix)
        delete m_rtArrowPix;
}

void MythScrollDialog::setArea(int w, int h)
{
    resizeContents(w, h);
}

void MythScrollDialog::setAreaMultiplied(int areaWTimes, int areaHTimes)
{
    if (areaWTimes < 1 || areaHTimes < 1) {
        VERBOSE(VB_IMPORTANT,
                QString("MythScrollDialog::setAreaMultiplied(%1,%2): "
                        "Warning, Invalid areaWTimes or areaHTimes. "
                        "Setting to 1.")
                .arg(areaWTimes).arg(areaHTimes));
        areaWTimes = areaHTimes = 1;
    }

    resizeContents(m_screenWidth*areaWTimes,
                   m_screenHeight*areaHTimes);
}

DialogCode MythScrollDialog::result(void) const
{
    return m_resCode;    
}

void MythScrollDialog::show()
{
    QScrollView::show();    
}

void MythScrollDialog::hide()
{
    if (isHidden())
        return;

    // Reimplemented to exit a modal when the dialog is hidden.
    QWidget::hide();
    QEventLoop *qteloop = QApplication::eventLoop();
    if (m_inLoop && qteloop)
    {
        m_inLoop = false;
        qteloop->exitLoop();
    }
}

DialogCode MythScrollDialog::exec(void)
{
    if (m_inLoop) 
    {
        std::cerr << "MythScrollDialog::exec: Recursive call detected."
                  << std::endl;
        return kDialogCodeRejected;
    }

    setResult(kDialogCodeRejected);

    show();

    m_inLoop = true;

    QEventLoop *qteloop = QApplication::eventLoop();
    if (!qteloop)
        return kDialogCodeRejected;

    qteloop->enterLoop();

    DialogCode res = result();

    return res;
}

void MythScrollDialog::done(int r)
{
    hide();
    setResult((DialogCode)r);
    close();
}

void MythScrollDialog::accept()
{
    done(kDialogCodeAccepted);
}

void MythScrollDialog::reject()
{
    done(kDialogCodeRejected);
}

void MythScrollDialog::setResult(DialogCode r)
{
    m_resCode = r;    
}

void MythScrollDialog::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;

    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE")
                reject();
            else if (action == "UP" || action == "LEFT")
            {
                if (focusWidget() &&
                    (focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                     focusWidget()->focusPolicy() == QWidget::WheelFocus))
                {
                }
                else
                    focusNextPrevChild(false);
            }
            else if (action == "DOWN" || action == "RIGHT")
            {
                if (focusWidget() &&
                    (focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                     focusWidget()->focusPolicy() == QWidget::WheelFocus)) 
                {
                }
                else
                    focusNextPrevChild(true);
            }
            else
                handled = false;
        }
    }
}

void MythScrollDialog::viewportPaintEvent(QPaintEvent *pe)
{
    if (!pe)
        return;

    QRect   er(pe->rect());
    QRegion reg(er);
    
    paintEvent(reg, er.x()+contentsX(), er.y()+contentsY(),
               er.width(), er.height());

    if (m_scrollMode == HScroll) {
        if (m_ltArrowPix && m_showLtArrow) {
            QPixmap pix(m_ltArrowRect.size());
            bitBlt(&pix, 0, 0, m_bgPixmap, m_ltArrowRect.x(), m_ltArrowRect.y());
            bitBlt(&pix, 0, 0, m_ltArrowPix);
            bitBlt(viewport(), m_ltArrowRect.x(), m_ltArrowRect.y(), &pix);
            reg -= m_ltArrowRect;
        }
        if (m_rtArrowPix && m_showRtArrow) {
            QPixmap pix(m_rtArrowRect.size());
            bitBlt(&pix, 0, 0, m_bgPixmap, m_rtArrowRect.x(), m_rtArrowRect.y());
            bitBlt(&pix, 0, 0, m_rtArrowPix);
            bitBlt(viewport(), m_rtArrowRect.x(), m_rtArrowRect.y(), &pix);
            reg -= m_rtArrowRect;
        }
    }
    else {
        if (m_upArrowPix && m_showUpArrow) {
            QPixmap pix(m_upArrowRect.size());
            bitBlt(&pix, 0, 0, m_bgPixmap, m_upArrowRect.x(), m_upArrowRect.y());
            bitBlt(&pix, 0, 0, m_upArrowPix);
            bitBlt(viewport(), m_upArrowRect.x(), m_upArrowRect.y(), &pix);
            reg -= m_upArrowRect;
        }
        if (m_dnArrowPix && m_showDnArrow) {
            QPixmap pix(m_dnArrowRect.size());
            bitBlt(&pix, 0, 0, m_bgPixmap, m_dnArrowRect.x(), m_dnArrowRect.y());
            bitBlt(&pix, 0, 0, m_dnArrowPix);
            bitBlt(viewport(), m_dnArrowRect.x(), m_dnArrowRect.y(), &pix);
            reg -= m_dnArrowRect;
        }
    }

    QPainter p(viewport());
    p.setClipRegion(reg);
    p.drawPixmap(0, 0, *m_bgPixmap, 0, 0, viewport()->width(),
                 viewport()->height());
    p.end();    
}

void MythScrollDialog::paintEvent(QRegion&, int , int , int , int )
{
}

void MythScrollDialog::setContentsPos(int x, int y)
{
    viewport()->setUpdatesEnabled(false);
    QScrollView::setContentsPos(x,y);
    viewport()->setUpdatesEnabled(true);
    updateContents();
}
    
      
