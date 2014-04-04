
#include <iostream>
#include <algorithm>
using namespace std;

#include <QCoreApplication>
#include <QCursor>
#include <QDialog>
#include <QDir>
#include <QLayout>
#include <QRegExp>
#include <QPixmap>
#include <QKeyEvent>
#include <QFrame>
#include <QPaintEvent>
#include <QPainter>
#include <QProgressBar>

#ifdef QWS
#include <qwindowsystem_qws.h>
#endif

#include "mythdialogs.h"
#include "mythwidgets.h"
#include "lcddevice.h"
#include "mythdbcon.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "mythlogging.h"
#include "mythcorecontext.h"

#ifdef _WIN32
#undef LoadImage
#endif

/** \class MythDialog
 *  \brief Base dialog for most dialogs in MythTV using the old UI
 */

MythDialog::MythDialog(MythMainWindow *parent, const char *name, bool setsize)
    : QFrame(parent), wmult(0.0), hmult(0.0),
      screenwidth(0), screenheight(0), xbase(0), ybase(0),
      m_parent(NULL), rescode(kDialogCodeAccepted), in_loop(false)
{
    setObjectName(name);
    if (!parent)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 "Trying to create a dialog without a parent.");
        return;
    }

    MythUIHelper *ui = GetMythUI();

    ui->GetScreenSettings(xbase, screenwidth, wmult,
                          ybase, screenheight, hmult);

    defaultBigFont = ui->GetBigFont();
    defaultMediumFont = ui->GetMediumFont();
    defaultSmallFont = ui->GetSmallFont();

    setFont(defaultMediumFont);

    if (setsize)
    {
        move(0, 0);
        setFixedSize(QSize(screenwidth, screenheight));
        GetMythUI()->ThemeWidget(this);
    }

    setAutoFillBackground(true);

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
        LOG(VB_GENERAL, LOG_ALERT,
                 QString("MythDialog::setResult(%1) "
                         "called with invalid DialogCode").arg(r));
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
        LOG(VB_GENERAL, LOG_ALERT,
                 QString("MythDialog::AcceptItem(%1) "
                         "called with negative index").arg(i));
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
        LOG(VB_GENERAL, LOG_ALERT,
                 "MythDialog::exec: Recursive call detected.");
        return kDialogCodeRejected;
    }

    setResult(kDialogCodeRejected);

    Show();

    in_loop = true;

    QEventLoop eventLoop;
    connect(this, SIGNAL(leaveModality()), &eventLoop, SLOT(quit()));
    eventLoop.exec();

    DialogCode res = result();

    return res;
}

void MythDialog::hide(void)
{
    if (isHidden())
        return;

    // Reimplemented to exit a modal when the dialog is hidden.
    QWidget::hide();
    if (in_loop)
    {
        in_loop = false;
        emit leaveModality();
    }
}

void MythDialog::keyPressEvent( QKeyEvent *e )
{
    bool handled = false;
    QStringList actions;

    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "ESCAPE")
            reject();
        else if (action == "UP" || action == "LEFT")
        {
            if (focusWidget() &&
                (focusWidget()->focusPolicy() == Qt::StrongFocus ||
                    focusWidget()->focusPolicy() == Qt::WheelFocus))
            {
            }
            else
                focusNextPrevChild(false);
        }
        else if (action == "DOWN" || action == "RIGHT")
        {
            if (focusWidget() &&
                (focusWidget()->focusPolicy() == Qt::StrongFocus ||
                    focusWidget()->focusPolicy() == Qt::WheelFocus))
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

    GetMythUI()->GetScreenSettings(wmult, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setPalette(parent->palette());
    popupForegroundColor = palette().color(foregroundRole());
    setFont(parent->font());

    hpadding = gCoreContext->GetNumSetting("PopupHeightPadding", 120);
    wpadding = gCoreContext->GetNumSetting("PopupWidthPadding", 80);

    vbox = new QVBoxLayout(this);
    vbox->setMargin((int)(10 * hmult));

    setAutoFillBackground(true);
    setWindowFlags(Qt::FramelessWindowHint);
}

MythPopupBox::MythPopupBox(MythMainWindow *parent, bool graphicPopup,
                           QColor popupForeground, QColor popupBackground,
                           QColor popupHighlight, const char *name)
            : MythDialog(parent, name, false)
{
    float wmult, hmult;

    GetMythUI()->GetScreenSettings(wmult, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setFrameStyle(QFrame::Box | QFrame::Plain);
    setPalette(parent->palette());
    setFont(parent->font());

    hpadding = gCoreContext->GetNumSetting("PopupHeightPadding", 120);
    wpadding = gCoreContext->GetNumSetting("PopupWidthPadding", 80);

    vbox = new QVBoxLayout(this);
    vbox->setMargin((int)(10 * hmult));

    if (!graphicPopup)
    {
        QPalette palette;
        palette.setColor(backgroundRole(), popupBackground);
        setPalette(palette);
    }
    else
        GetMythUI()->ThemeWidget(this);

    QPalette palette;
    palette.setColor(foregroundRole(), popupHighlight);
    setPalette(palette);

    popupForegroundColor = popupForeground;
    setAutoFillBackground(true);
    setWindowFlags(Qt::FramelessWindowHint);
}


bool MythPopupBox::focusNextPrevChild(bool next)
{
    // -=>TODO: Temp fix... should re-evalutate/re-code.

    QList<QWidget *> objList = this->findChildren<QWidget *>();

    QWidget *pCurr    = focusWidget();
    QWidget *pNew     = NULL;
    int      nCurrIdx = -1;
    int      nIdx;

    for (nIdx = 0; nIdx < objList.size(); ++nIdx )
    {
        if (objList[ nIdx ] == pCurr)
        {
            nCurrIdx = nIdx;
            break;
        }
    }

    if (nCurrIdx == -1)
        return false;

    nIdx = nCurrIdx;

    do
    {
        if (next)
        {
            ++nIdx;

            if (nIdx == objList.size())
                nIdx = 0;
        }
        else
        {
            --nIdx;

            if (nIdx < 0)
                nIdx = objList.size() -1;
        }

        pNew = objList[ nIdx ];

        if (pNew && !pNew->focusProxy() && pNew->isVisibleTo( this ) &&
            pNew->isEnabled() && (pNew->focusPolicy() != Qt::NoFocus))
        {
            pNew->setFocus();
            return true;
        }
    }
    while (nIdx != nCurrIdx);

    return false;

#if 0
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
#endif
}

void MythPopupBox::addWidget(QWidget *widget, bool setAppearance)
{
    if (setAppearance == true)
    {
         widget->setPalette(palette());
         widget->setFont(font());
    }

    if (widget->metaObject()->className() == QString("MythLabel"))
    {
        QPalette palette;
        palette.setColor(widget->foregroundRole(), popupForegroundColor);
        widget->setPalette(palette);
    }

    vbox->addWidget(widget);
}

MythLabel *MythPopupBox::addLabel(QString caption, LabelSize size, bool wrap)
{
    MythLabel *label = new MythLabel(caption, this);
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
        Qt::Alignment align = (QChar::DirAL == text_dir) ?
            Qt::AlignRight : Qt::AlignLeft;
        label->setAlignment(align);
        label->setWordWrap(true);
    }

    label->setWordWrap(true);
    addWidget(label, false);
    return label;
}

QAbstractButton *MythPopupBox::addButton(QString caption, QObject *target,
                                 const char *slot)
{
    if (!target)
    {
        target = this;
        slot = SLOT(defaultButtonPressedHandler());
    }

    MythPushButton *button = new MythPushButton(caption, this);
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
    QList< QObject* > objlist = children();

    for (QList< QObject* >::Iterator it  = objlist.begin();
                                     it != objlist.end();
                                   ++it )
    {
        QObject *objs = *it;

        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            widget->adjustSize();
        }
    }

    ensurePolished();

    int x = 0, y = 0, maxw = 0, poph = 0;

    for (QList< QObject* >::Iterator it  = objlist.begin();
                                     it != objlist.end();
                                   ++it )
    {
        QObject *objs = *it;

        if (objs->isWidgetType())
        {
            QString objname = objs->objectName();
            if (objname != "nopopsize")
            {
                // little extra padding for these guys
                if (objs->metaObject()->className() ==
                    QString("MythListBox"))
                {
                    poph += (int)(25 * hmult);
                }

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
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];

        if (action == "ESCAPE")
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
    QList< QObject* > objlist = children();

    int i = 0;
    bool foundbutton = false;

    for (QList< QObject* >::Iterator it  = objlist.begin();
                                     it != objlist.end();
                                   ++it )
    {
        QObject *objs = *it;

        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            if (widget->metaObject()->className() ==
                QString("MythPushButton"))
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
    i = 0;
    for (QList< QObject* >::Iterator it  = objlist.begin();
                                     it != objlist.end();
                                   ++it )
    {
        QObject *objs = *it;

        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            if (widget->metaObject()->className() ==
                QString("MythPushButton"))
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

    LOG(VB_GENERAL, LOG_ALERT, "We should never get here!");
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

    MythPopupBox *popup = new MythPopupBox(parent, title.toLatin1().constData());

    popup->addLabel(message, MythPopupBox::Medium, true);
    QAbstractButton *okButton = popup->addButton(button_msg, popup, SLOT(accept()));
    okButton->setFocus();
    bool ret = (kDialogCodeAccepted == popup->ExecPopup());

    popup->hide();
    popup->deleteLater();

    return ret;
}

bool MythPopupBox::showGetTextPopup(MythMainWindow *parent, QString title,
                                    QString message, QString& text)
{
    MythPopupBox *popup = new MythPopupBox(parent, title.toLatin1().constData());

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
        text = textEdit->text();

    popup->hide();
    popup->deleteLater();

    return ok;
}

DialogCode MythPopupBox::Show2ButtonPopup(
    MythMainWindow *parent,
    const QString &title, const QString &message,
    const QString &button1msg, const QString &button2msg,
    DialogCode default_button)
{
    QStringList buttonmsgs;
    buttonmsgs += (button1msg.isEmpty()) ?
        QString("Button 1") : button1msg;
    buttonmsgs += (button2msg.isEmpty()) ?
        QString("Button 2") : button2msg;
    return ShowButtonPopup(
        parent, title, message, buttonmsgs, default_button);
}

DialogCode MythPopupBox::ShowButtonPopup(
    MythMainWindow    *parent,
    const QString     &title,
    const QString     &message,
    const QStringList &buttonmsgs,
    DialogCode         default_button)
{
    MythPopupBox *popup = new MythPopupBox(parent, title.toLatin1().constData());

    popup->addLabel(message, Medium, true);
    popup->addLabel("");

    const int def = CalcItemIndex(default_button);
    for (int i = 0; i < buttonmsgs.size(); i++ )
    {
        QAbstractButton *but = popup->addButton(buttonmsgs[i]);
        if (def == i)
            but->setFocus();
    }

    DialogCode ret = popup->ExecPopup();

    popup->hide();
    popup->deleteLater();

    return ret;
}

MythProgressDialog::MythProgressDialog(
    const QString &message, int totalSteps,
    bool cancelButton, const QObject *target, const char *slot)
    : MythDialog(GetMythMainWindow(), "progress", false)
{
    setObjectName("MythProgressDialog");
    int screenwidth, screenheight;
    float wmult, hmult;

    GetMythUI()->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setFont(GetMythUI()->GetMediumFont());

    GetMythUI()->ThemeWidget(this);

    int yoff = screenheight / 3;
    int xoff = screenwidth / 10;
    setGeometry(xoff, yoff, screenwidth - xoff * 2, yoff);
    setFixedSize(QSize(screenwidth - xoff * 2, yoff));

    msglabel = new MythLabel();
    msglabel->setText(message);

    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addWidget(msglabel);

    progress = new QProgressBar();
    progress->setRange(0, totalSteps);

    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->addWidget(progress);

    if (cancelButton && slot && target)
    {
        MythPushButton *button = new MythPushButton(
            QObject::tr("Cancel"), NULL);
        button->setFocus();
        hlayout->addWidget(button);
        connect(button, SIGNAL(pressed()), target, slot);
    }

    setTotalSteps(totalSteps);

    if (LCD *lcddev = LCD::Get())
    {
        QList<LCDTextItem> textItems;

        textItems.append(LCDTextItem(1, ALIGN_CENTERED, message, "Generic",
                                     false));
        lcddev->switchToGeneric(textItems);
    }

    hlayout->setSpacing(5);

    vlayout->setMargin((int)(15 * wmult));
    vlayout->setStretchFactor(msglabel, 5);

    QWidget *hbox = new QWidget();
    hbox->setLayout(hlayout);
    vlayout->addWidget(hbox);

    QFrame *vbox = new QFrame(this);
    vbox->setObjectName(objectName() + "_vbox");
    vbox->setLineWidth(3);
    vbox->setMidLineWidth(3);
    vbox->setFrameShape(QFrame::Panel);
    vbox->setFrameShadow(QFrame::Raised);
    vbox->setLayout(vlayout);

    QVBoxLayout *lay = new QVBoxLayout();
    lay->addWidget(vbox);
    setLayout(lay);

    show();

    qApp->processEvents();
}

MythProgressDialog::~MythProgressDialog()
{
}

void MythProgressDialog::deleteLater(void)
{
    hide();
    MythDialog::deleteLater();
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
    progress->setValue(curprogress);
    if (curprogress % steps == 0)
    {
        qApp->processEvents();
        if (LCD *lcddev = LCD::Get())
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
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        if (action == "ESCAPE")
            handled = true;
    }

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void MythProgressDialog::setTotalSteps(int totalSteps)
{
    m_totalSteps = totalSteps;
    progress->setRange(0, totalSteps);
    steps = totalSteps / 1000;
    if (steps == 0)
        steps = 1;
}
