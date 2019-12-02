// -*- Mode: c++ -*-

// MythTV headers
#include "inputinfo.h"

void InputInfo::Clear(void)
{
    InputInfo blank;
    *this = blank;
}

#define NEXT() do { ++it; if (it == end) return false; } while (false)
bool InputInfo::FromStringList(QStringList::const_iterator &it,
                               const QStringList::const_iterator& end)
{
    if (it == end)
        return false;

    m_name          = *it;
    m_name          = (m_name == "<EMPTY>") ? QString() : m_name;
    NEXT();

    m_sourceid      = (*it).toUInt(); NEXT();
    m_inputid       = (*it).toUInt(); NEXT();
    m_mplexid       = (*it).toUInt(); NEXT();
    m_liveTvOrder   = (*it).toUInt(); NEXT();

    m_displayName   = *it;
    m_displayName   = (m_displayName == "<EMPTY>") ? QString() : m_displayName;
    NEXT();

    m_recPriority   = (*it).toInt(); NEXT();
    m_scheduleOrder = (*it).toUInt(); NEXT();
    m_quickTune     = ((*it).toUInt() != 0U); NEXT();
    m_chanid        = (*it).toUInt(); ++it;

    return true;
}
#undef NEXT

void InputInfo::ToStringList(QStringList &list) const
{
    list.push_back(m_name.isEmpty() ? "<EMPTY>" : m_name);
    list.push_back(QString::number(m_sourceid));
    list.push_back(QString::number(m_inputid));
    list.push_back(QString::number(m_mplexid));
    list.push_back(QString::number(m_liveTvOrder));
    list.push_back(m_displayName.isEmpty() ? "<EMPTY>" : m_displayName);
    list.push_back(QString::number(m_recPriority));
    list.push_back(QString::number(m_scheduleOrder));
    list.push_back(QString::number(m_quickTune));
    list.push_back(QString::number(m_chanid));
}

