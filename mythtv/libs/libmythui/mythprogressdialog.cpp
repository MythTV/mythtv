

#include "mythprogressdialog.h"

// libmythbase headers
#include "libmythbase/mythlogging.h"

// libmythui headers
#include "mythuitext.h"
#include "mythuiprogressbar.h"

const QEvent::Type ProgressUpdateEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

// Force this class to have a vtable so that dynamic_cast works.
// NOLINTNEXTLINE(modernize-use-equals-default)
ProgressUpdateEvent::~ProgressUpdateEvent()
{
}

MythUIBusyDialog::MythUIBusyDialog(const QString &message,
                             MythScreenStack *parent, const char *name)
         : MythScreenType(parent, name, false)
{
    if (!message.isEmpty())
        m_message = message;
    else
        m_message = tr("Please Wait...");
    m_origMessage = m_message;
}

bool MythUIBusyDialog::Create(void)
{
    if (!CopyWindowFromBase("MythBusyDialog", this))
        return false;

    m_messageText = dynamic_cast<MythUIText *>(GetChild("message"));

    if (m_messageText)
        m_messageText->SetText(m_message);

    return true;
}

void MythUIBusyDialog::SetMessage(const QString &message)
{
    m_newMessageLock.lock();
    m_newMessage = message;
    m_haveNewMessage = true;
    m_newMessageLock.unlock();
}

void MythUIBusyDialog::Reset(void)
{
    SetMessage(m_origMessage);
}

void MythUIBusyDialog::Pulse(void)
{
    if (m_haveNewMessage && m_messageText)
    {
        m_newMessageLock.lock();
        m_message = m_newMessage;
        m_messageText->SetText(m_message);
        m_newMessageLock.unlock();
    }

    MythUIType::Pulse();
}

bool MythUIBusyDialog::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("qt", event, actions, false);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            // eat the escape keypress
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

MythUIBusyDialog  *ShowBusyPopup(const QString &message)
{
    QString                  LOC = "ShowBusyPopup('" + message + "') - ";
    MythUIBusyDialog        *pop = nullptr;
    static MythScreenStack  *s_stk = nullptr;


    if (!s_stk)
    {
        MythMainWindow *win = GetMythMainWindow();

        if (win)
            s_stk = win->GetStack("popup stack");
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "no main window?");
            return nullptr;
        }

        if (!s_stk)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "no popup stack? "
                                           "Is there a MythThemeBase?");
            return nullptr;
        }
    }

    pop = new MythUIBusyDialog(message, s_stk, "showBusyPopup");
    if (pop->Create())
        s_stk->AddScreen(pop);

    return pop;
}
//---------------------------------------------------------

bool MythUIProgressDialog::Create(void)
{
    if (!CopyWindowFromBase("MythProgressDialog", this))
        return false;

    m_messageText = dynamic_cast<MythUIText *>(GetChild("message"));
    m_progressText = dynamic_cast<MythUIText *>(GetChild("progresstext"));
    m_progressBar = dynamic_cast<MythUIProgressBar *>(GetChild("progressbar"));

    if (m_messageText)
        m_messageText->SetText(m_message);

    return true;
}

bool MythUIProgressDialog::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("qt", event, actions, false);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "ESCAPE")
        {
            // eat the escape keypress
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

void MythUIProgressDialog::customEvent(QEvent *event)
{
    if (event->type() == ProgressUpdateEvent::kEventType)
    {
        auto *pue = dynamic_cast<ProgressUpdateEvent*>(event);
        if (!pue)
        {
            LOG(VB_GENERAL, LOG_ERR,
                    "Error, event claims to be a progress update but fails "
                    "to cast");
            return;
        }

        QString message = pue->GetMessage();
        if (!message.isEmpty())
            m_message = message;
        uint total = pue->GetTotal();
        if (total > 0)
            m_total = total;
        m_count = pue->GetCount();
        UpdateProgress();
    }
}

void MythUIProgressDialog::SetTotal(uint total)
{
    m_total = total;
    UpdateProgress();
}

void MythUIProgressDialog::SetProgress(uint count)
{
    m_count = count;
    UpdateProgress();
}

void MythUIProgressDialog::SetMessage(const QString &message)
{
    m_message = message;
    UpdateProgress();
}

void MythUIProgressDialog::UpdateProgress()
{
    if (m_messageText)
        m_messageText->SetText(m_message);

    if (m_total == 0)
        return;

    if (m_count > m_total)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Progress count (%1) is higher "
                                         "than total (%2)")
                                      .arg(m_count) .arg(m_total));
        return;
    }

     if (m_progressBar)
     {
         m_progressBar->SetTotal(m_total);
         m_progressBar->SetUsed(m_count);
     }

    uint percentage = (uint)(((float)m_count/(float)m_total) * 100.0F);

    if (m_progressText)
        m_progressText->SetText(QString("%1%").arg(percentage));
}
