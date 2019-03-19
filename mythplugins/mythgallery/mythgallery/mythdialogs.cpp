
#include <iostream>
#include <algorithm>
using namespace std;

#include <QApplication>
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
#include <QAbstractButton>

#ifdef QWS
#include <qwindowsystem_qws.h>
#endif

#include "mythdialogs.h"
#include "lcddevice.h"
#include "mythdbcon.h"
#include "mythfontproperties.h"
#include "mythuihelper.h"
#include "mythlogging.h"
#include "mythcorecontext.h"
#include "mythmainwindow.h"

#ifdef _WIN32
#undef LoadImage
#endif

static bool inline IsEGLFS()
{
  return qApp->platformName().contains("egl");
}

/** \class MythDialog
 *  \brief Base dialog for most dialogs in MythTV using the old UI
 */

MythDialog::MythDialog(MythMainWindow *parent, const char *name, bool setsize)
    : QFrame(IsEGLFS() ? nullptr : parent)
{
    setObjectName(name);
    if (!parent)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 "Trying to create a dialog without a parent.");
        return;
    }

    MythUIHelper *ui = GetMythUI();

    ui->GetScreenSettings(m_xbase, m_screenwidth,  m_dlgwmult,
                          m_ybase, m_screenheight, m_dlghmult);

    m_defaultBigFont = ui->GetBigFont();
    m_defaultMediumFont = ui->GetMediumFont();
    m_defaultSmallFont = ui->GetSmallFont();

    setFont(m_defaultMediumFont);

    if (setsize)
    {
        move(0, 0);
        setFixedSize(QSize(m_screenwidth, m_screenheight));
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
        m_parent = nullptr;
    }
}

void MythDialog::setNoErase(void)
{
}

bool MythDialog::onMediaEvent(MythMediaDevice* /*mediadevice*/)
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

    m_rescode = r;
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

    done((DialogCode)((int)kDialogCodeListStart + i));
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
    if (m_in_loop)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 "MythDialog::exec: Recursive call detected.");
        return kDialogCodeRejected;
    }

    setResult(kDialogCodeRejected);

    Show();

    m_in_loop = true;

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
    if (m_in_loop)
    {
        m_in_loop = false;
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
