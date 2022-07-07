// -*- Mode: c++ -*-

// MythTV headers
#include "inputinfo.h"

void InputInfo::Clear(void)
{
    InputInfo blank;
    *this = blank;
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NEXT() do { ++it; if (it == end) return false; } while (false)
bool InputInfo::FromStringList(QStringList::const_iterator &it,
                               const QStringList::const_iterator& end)
{
    if (it == end)
        return false;

    m_name          = *it;
    m_name          = (m_name == "<EMPTY>") ? QString() : m_name;
    NEXT();

    m_sourceId      = (*it).toUInt(); NEXT();
    m_inputId       = (*it).toUInt(); NEXT();
    m_mplexId       = (*it).toUInt(); NEXT();
    m_liveTvOrder   = (*it).toUInt(); NEXT();

    m_displayName   = *it;
    m_displayName   = (m_displayName == "<EMPTY>") ? QString() : m_displayName;
    NEXT();

    m_recPriority   = (*it).toInt(); NEXT();
    m_scheduleOrder = (*it).toUInt(); NEXT();
    m_quickTune     = ((*it).toUInt() != 0U); NEXT();
    m_chanId        = (*it).toUInt(); ++it;

    return true;
}
#undef NEXT

void InputInfo::ToStringList(QStringList &list) const
{
    list.push_back(m_name.isEmpty() ? "<EMPTY>" : m_name);
    list.push_back(QString::number(m_sourceId));
    list.push_back(QString::number(m_inputId));
    list.push_back(QString::number(m_mplexId));
    list.push_back(QString::number(m_liveTvOrder));
    list.push_back(m_displayName.isEmpty() ? "<EMPTY>" : m_displayName);
    list.push_back(QString::number(m_recPriority));
    list.push_back(QString::number(m_scheduleOrder));
    list.push_back(QString::number(static_cast<int>(m_quickTune)));
    list.push_back(QString::number(m_chanId));
}

