// C/C++ headers
#include <map>

// Myth headers
#include <mythtv/mythcontext.h>

// Mythui headers
#include <mythtv/libmythui/mythuihelper.h>
#include <mythtv/libmythui/mythmainwindow.h>
#include <mythtv/libmythui/mythscreenstack.h>
#include <mythtv/libmythui/mythdialogbox.h>

// MythVideo headers
#include "parentalcontrols.h"


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

///////////////////////////////////////////////////////////////

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
            if (p != m_passwords.end() && !p->second.isEmpty())
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
            if (p != m_passwords.end() && !p->second.isEmpty())
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


///////////////////////////////////////////////////////////////////////

ParentalLevel::ParentalLevel(ParentalLevel::Level pl)
              : QObject(),  m_currentLevel(pl), m_hitlimit(false)
{
}

ParentalLevel::ParentalLevel(int pl)
              : QObject(), m_hitlimit(false)
{
    m_currentLevel = toParentalLevel(pl);
}

ParentalLevel::ParentalLevel(const ParentalLevel &rhs)
              : QObject(), m_hitlimit(false)
{
    *this = rhs;
}

ParentalLevel &ParentalLevel::operator=(const ParentalLevel &rhs)
{
    if (&rhs != this)
    {
        m_currentLevel = rhs.m_currentLevel;
    }
    return *this;
}

ParentalLevel &ParentalLevel::operator=(Level pl)
{
    m_currentLevel = boundedParentalLevel(pl);
    return *this;
}

ParentalLevel &ParentalLevel::operator++()
{
    Level last = m_currentLevel;
    m_currentLevel = nextParentalLevel(m_currentLevel);
    if (m_currentLevel == last)
        m_hitlimit = true;
    return *this;
}

ParentalLevel &ParentalLevel::operator+=(int amount)
{
    m_currentLevel = toParentalLevel(m_currentLevel + amount);
    return *this;
}

ParentalLevel &ParentalLevel::operator--()
{
    Level prev = m_currentLevel;
    m_currentLevel = prevParentalLevel(m_currentLevel);
    if (m_currentLevel == prev)
        m_hitlimit = true;
    return *this;
}

ParentalLevel &ParentalLevel::operator-=(int amount)
{
    m_currentLevel = toParentalLevel(m_currentLevel - amount);
    return *this;
}

ParentalLevel::Level ParentalLevel::GetLevel() const
{
    return m_currentLevel;
}

void ParentalLevel::checkPassword()
{
    PasswordManager pm;
    pm.Add(ParentalLevel::plHigh, gContext->GetSetting("VideoAdminPassword"));
    pm.Add(ParentalLevel::plMedium,
           gContext->GetSetting("VideoAdminPasswordThree"));
    pm.Add(ParentalLevel::plLow, gContext->GetSetting("VideoAdminPasswordTwo"));

    bool succeeded = false;

    // No password for level 1 and you can always switch down from your
    // current level.
    if (m_currentLevel == ParentalLevel::plLowest ||
        m_currentLevel <= m_previousLevel)
        succeeded = true;

    // If there isn't a password at the current level, and
    // none of the levels below, we are done.
    // The assumption is that if you password protected lower levels,
    // and a higher level does not have a password it is something
    // you've overlooked (rather than intended).
    if (!succeeded && pm.FirstAtOrBelow(m_currentLevel).isEmpty())
        succeeded = true;

    // See if we recently (and successfully) asked for a password
    QString last_time_stamp = gContext->GetSetting("VideoPasswordTime");
    int last_parent_lvl = gContext->GetNumSetting("VideoPasswordLevel", -1);

    if (!succeeded)
    {
        if (last_time_stamp.isEmpty() || last_parent_lvl == -1)
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

            if (last_parent_lvl >= m_currentLevel &&
                last_time.secsTo(curr_time) < 120)
            {
                // Two minute window
                last_time_stamp = curr_time.toString(Qt::ISODate);
                gContext->SetSetting("VideoPasswordTime", last_time_stamp);
                succeeded = true;
            }
        }
    }

    if (!succeeded)
    {
        m_passwords.clear();

        m_passwords = pm.AtOrAbove(m_currentLevel);

        // If there isn't a password for this level or higher levels, treat
        // the next lower password as valid. This is only done so people
        // cannot lock themselves out of the setup.
        if (m_passwords.isEmpty())
        {
            QString pw = pm.FirstAtOrBelow(m_currentLevel);
            if (!pw.isEmpty())
                m_passwords.push_back(pw);
        }

        // There are no suitable passwords.
        if (m_passwords.isEmpty())
            succeeded = true;
    }

    if (succeeded)
    {
        emit LevelChanged();
        return;
    }

    // If we got here, there is a password, and there's no backing down.
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    MythTextInputDialog *pwd = new MythTextInputDialog(popupStack,
                                                QObject::tr("Parental Pin:"),
                                                FilterNone, true);

    connect(pwd, SIGNAL(haveResult(QString)), SLOT(verifyPassword(QString)));
    connect(pwd, SIGNAL(Exiting()), SLOT(verifyPassword()));

    if (pwd->Create())
        popupStack->AddScreen(pwd, false);
}

bool ParentalLevel::verifyPassword(const QString &password)
{
    bool ok = false;

    for (int i = 0; i < m_passwords.size(); ++i)
    {
        if (password == m_passwords.at(i))
        {
            ok = true;
            break;
        }
    }

    if (ok)
    {
        QString time_stamp = QDateTime::currentDateTime().toString(Qt::ISODate);

        gContext->SaveSetting("VideoPasswordTime", time_stamp);
        gContext->SaveSetting("VideoPasswordLevel", m_currentLevel);
    }
    else
    {
        // If we fail to verify the password, then revert to the previous level
        // or the lowest level
        if (m_currentLevel > m_previousLevel)
            m_currentLevel = m_previousLevel;
        else
            m_currentLevel = plLowest;
    }

    emit LevelChanged();

    return ok;
}

void ParentalLevel::SetLevel(Level pl)
{
    m_previousLevel = m_currentLevel;
    m_currentLevel = pl;
    checkPassword();
}
