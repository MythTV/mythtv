#include <map>

#include <Q3Frame>
#include <QLabel>
#include <QKeyEvent>

#include <mythtv/mythcontext.h>
#include <mythtv/uitypes.h>
#include <mythtv/libmythui/mythuihelper.h>

#include "parentalcontrols.h"

namespace
{

ParentalLevel::Level boundedParentalLevel(ParentalLevel::Level pl)
{
    if (pl < ParentalLevel::plNone)
        return ParentalLevel::plNone;
    else if (pl > ParentalLevel::plHigh)
        return ParentalLevel::plHigh;

    return pl;
}

ParentalLevel::Level nextParentalLevel(ParentalLevel::Level cpl)
{
    ParentalLevel::Level rpl(cpl);
    switch (cpl)
    {
        case ParentalLevel::plNone: { rpl = ParentalLevel::plLowest; break; }
        case ParentalLevel::plLowest: { rpl = ParentalLevel::plLow; break; }
        case ParentalLevel::plLow: { rpl = ParentalLevel::plMedium; break; }
        case ParentalLevel::plMedium: { rpl = ParentalLevel::plHigh; break; }
        case ParentalLevel::plHigh: { rpl = ParentalLevel::plHigh; break; }
    }

    return boundedParentalLevel(rpl);
}

ParentalLevel::Level prevParentalLevel(ParentalLevel::Level cpl)
{
    ParentalLevel::Level rpl(cpl);
    switch (cpl)
    {
        case ParentalLevel::plNone: { rpl = ParentalLevel::plNone; break; }
        case ParentalLevel::plLowest: { rpl = ParentalLevel::plLowest; break; }
        case ParentalLevel::plLow: { rpl = ParentalLevel::plLowest; break; }
        case ParentalLevel::plMedium: { rpl = ParentalLevel::plLow; break; }
        case ParentalLevel::plHigh: { rpl = ParentalLevel::plMedium; break; }
    }

    return boundedParentalLevel(rpl);
}

ParentalLevel::Level toParentalLevel(int pl)
{
    return boundedParentalLevel(static_cast<ParentalLevel::Level>(pl));
}

}

ParentalLevel::ParentalLevel(ParentalLevel::Level pl) : m_pl(pl),
    m_hitlimit(false)
{
}

ParentalLevel::ParentalLevel(int pl) : m_hitlimit(false)
{
    m_pl = toParentalLevel(pl);
}

ParentalLevel::ParentalLevel(const ParentalLevel &rhs) : m_hitlimit(false)
{
    *this = rhs;
}

ParentalLevel &ParentalLevel::operator=(const ParentalLevel &rhs)
{
    if (&rhs != this)
    {
        m_pl = rhs.m_pl;
    }
    return *this;
}

ParentalLevel &ParentalLevel::operator=(Level pl)
{
    m_pl = boundedParentalLevel(pl);
    return *this;
}

ParentalLevel &ParentalLevel::operator++()
{
    Level last = m_pl;
    m_pl = nextParentalLevel(m_pl);
    if (m_pl == last)
        m_hitlimit = true;
    return *this;
}

ParentalLevel &ParentalLevel::operator+=(int amount)
{
    m_pl = toParentalLevel(m_pl + amount);
    return *this;
}

ParentalLevel &ParentalLevel::operator--()
{
    Level prev = m_pl;
    m_pl = prevParentalLevel(m_pl);
    if (m_pl == prev)
        m_hitlimit = true;
    return *this;
}

ParentalLevel &ParentalLevel::operator-=(int amount)
{
    m_pl = toParentalLevel(m_pl - amount);
    return *this;
}

ParentalLevel::Level ParentalLevel::GetLevel() const
{
    return m_pl;
}

bool operator!=(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() != rhs.GetLevel();
}

bool operator==(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() == rhs.GetLevel();
}

bool operator<(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() < rhs.GetLevel();
}

bool operator>(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() > rhs.GetLevel();
}

bool operator<=(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() <= rhs.GetLevel();
}

bool operator>=(const ParentalLevel &lhs, const ParentalLevel &rhs)
{
    return lhs.GetLevel() >= rhs.GetLevel();
}

///////////////////////////////////////////////////////////////////////

class MythMultiPasswordDialog : public MythDialog
{
    Q_OBJECT
  public:
    MythMultiPasswordDialog(const QString &message, const QStringList &pwlist,
                            MythMainWindow *lparent, const char *lname = 0);

  private slots:
    void checkPassword(const QString &password);

  protected:
    ~MythMultiPasswordDialog(); // use deleteLater for thread safety
    void keyPressEvent(QKeyEvent *e);

  private:
    MythLineEdit        *m_passwordEditor;
    QStringList         m_passwords;
};

MythMultiPasswordDialog::MythMultiPasswordDialog(const QString &message,
                                                 const QStringList &pwlist,
                                                 MythMainWindow *lparent,
                                                 const char *lname) :
        MythDialog(lparent, lname, false), m_passwords(pwlist)
{
    int textWidth = fontMetrics().width(message);
    int totalWidth = textWidth + 175;

    GetMythUI()->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    setGeometry((screenwidth - 250) / 2,
                (screenheight - 50) / 2, totalWidth, 50);

    Q3Frame *outside_border = new Q3Frame(this);
    outside_border->setGeometry(0, 0, totalWidth, 50);
    outside_border->setFrameStyle(Q3Frame::Panel | Q3Frame::Raised);
    outside_border->setLineWidth(4);

    QLabel *message_label = new QLabel(message, this);
    message_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    message_label->setGeometry(15, 10, textWidth, 30);
    message_label->setBackgroundOrigin(ParentOrigin);

    m_passwordEditor = new MythLineEdit(this);
    m_passwordEditor->setEchoMode(QLineEdit::Password);
    m_passwordEditor->setGeometry(textWidth + 20, 10, 135, 30);
    m_passwordEditor->setBackgroundOrigin(ParentOrigin);
    m_passwordEditor->setAllowVirtualKeyboard(false);
    connect(m_passwordEditor, SIGNAL(textChanged(const QString &)),
            this, SLOT(checkPassword(const QString &)));

    setActiveWindow();
    m_passwordEditor->setFocus();
}

void MythMultiPasswordDialog::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        bool handled = false;
        for (QStringList::const_iterator p = actions.begin();
             p != actions.end() && !handled; ++p)
        {
            if (*p == "ESCAPE")
            {
                handled = true;
                MythDialog::keyPressEvent(e);
            }
        }
    }
}

void MythMultiPasswordDialog::checkPassword(const QString &password)
{
    for (QStringList::const_iterator p = m_passwords.begin();
         p != m_passwords.end(); ++p)
    {
        if (password == *p)
            accept();
    }
}

MythMultiPasswordDialog::~MythMultiPasswordDialog()
{
}

///////////////////////////////////////////////////////////////////////

namespace
{

    class PasswordManager
    {
      private:
        typedef std::map<ParentalLevel::Level, QString> pws;

      public:
        void Add(ParentalLevel::Level level, const QString &password)
        {
            m_passwords.insert(pws::value_type(level, password));
        }

        QStringList AtOrAbove(ParentalLevel::Level level)
        {
            QStringList ret;
            for (ParentalLevel i = level;
                 i <= ParentalLevel::plHigh && i.good(); ++i)
            {
                pws::const_iterator p = m_passwords.find(i.GetLevel());
                if (p != m_passwords.end() && p->second.length())
                    ret.push_back(p->second);
            }

            return ret;
        }

        QString FirstAtOrBelow(ParentalLevel::Level level)
        {
            QString ret;
            for (ParentalLevel i = level;
                 i >= ParentalLevel::plLow && i.good(); --i)
            {
                pws::const_iterator p = m_passwords.find(i.GetLevel());
                if (p != m_passwords.end() && p->second.length())
                {
                    ret = p->second;
                    break;
                }
            }

            return ret;
        }

      private:
        pws m_passwords;
    };

}

bool checkParentPassword(ParentalLevel::Level to_level,
                         ParentalLevel::Level current)
{
    PasswordManager pm;
    pm.Add(ParentalLevel::plHigh, gContext->GetSetting("VideoAdminPassword"));
    pm.Add(ParentalLevel::plMedium,
           gContext->GetSetting("VideoAdminPasswordThree"));
    pm.Add(ParentalLevel::plLow, gContext->GetSetting("VideoAdminPasswordTwo"));

    ParentalLevel which_level(to_level);

    // No password for level 1 and you can always switch down from your
    // current level.
    if (which_level == ParentalLevel::plLowest ||
        which_level <= ParentalLevel(current))
        return true;

    // If there isn't a password at the current level, and
    // none of the levels below, we are done.
    // The assumption is that if you password protected lower levels,
    // and a higher level does not have a password it is something
    // you've overlooked (rather than intended).
    if (!pm.FirstAtOrBelow(which_level.GetLevel()).length())
        return true;

    // See if we recently (and successfully) asked for a password
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    int last_parent_lvl = gContext->GetNumSetting("VideoPasswordLevel", -1);

    if (!last_time_stamp.length() || last_parent_lvl == -1)
    {
        VERBOSE(VB_IMPORTANT,
                QString("%1: Could not read password/pin time "
                        "stamp. This is only an issue if it "
                        "happens repeatedly.").arg(__FILE__));
    }
    else
    {
        QDateTime curr_time = QDateTime::currentDateTime();
        QDateTime last_time =
                QDateTime::fromString(last_time_stamp, Qt::ISODate);

        if (ParentalLevel(last_parent_lvl) >= which_level &&
            last_time.secsTo(curr_time) < 120)
        {
            // Two minute window
            last_time_stamp = curr_time.toString(Qt::ISODate);
            gContext->SetSetting("VideoPasswordTime", last_time_stamp);
            gContext->SaveSetting("VideoPasswordTime", last_time_stamp);
            return true;
        }
    }

    QStringList valid_passwords = pm.AtOrAbove(which_level.GetLevel());

    // If there isn't a password for this level or higher levels, treat
    // the next lower password as valid. This is only done so people
    // cannot lock themselves out of the setup.
    if (!valid_passwords.size())
    {
        QString pw = pm.FirstAtOrBelow(which_level.GetLevel());
        if (pw.length())
            valid_passwords.push_back(pw);
    }

    // There are no suitable passwords.
    if (!valid_passwords.size())
        return true;

    // If we got here, there is a password, and there's no backing down.
    MythMultiPasswordDialog *pwd =
            new MythMultiPasswordDialog(QObject::tr("Parental Pin:"),
                                        valid_passwords,
                                        gContext->GetMainWindow());
    bool ok = (kDialogCodeRejected != pwd->exec());
    pwd->deleteLater();

    if (ok)
    {
        last_time_stamp = QDateTime::currentDateTime().toString(Qt::ISODate);

        gContext->SetSetting("VideoPasswordTime", last_time_stamp);
        gContext->SaveSetting("VideoPasswordTime", last_time_stamp);

        gContext->SetSetting("VideoPasswordLevel",
                             QString::number(which_level.GetLevel()));
        gContext->SaveSetting("VideoPasswordLevel", which_level.GetLevel());

        return true;
    }

    return false;
}

#include "parentalcontrols.moc"
