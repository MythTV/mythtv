#include <algorithm>
#include <utility>

#include "libmythbase/mythlogging.h"

#include "signalmonitorvalue.h"

bool SignalMonitorValue::run_static_init = true;
QStringList SignalMonitorValue::ERROR_NO_CHANNEL;
QStringList SignalMonitorValue::ERROR_NO_LINK;
QStringList SignalMonitorValue::SIGNAL_LOCK;

#define DEBUG_SIGNAL_MONITOR_VALUE 1

/** \fn SignalMonitorValue::Init()
 *  \brief Initializes the some static constants needed by SignalMonitorValue.
 *
 *   This isn't done automatically because we need to translate the messages.
 */
void SignalMonitorValue::Init()
{
    if (run_static_init)
    {
        run_static_init = false;
        ERROR_NO_CHANNEL<<"error"<<tr("Could not open tuner device");
        ERROR_NO_LINK<<"error"<<tr("Bad connection to backend");

        SignalMonitorValue slock(
            QCoreApplication::translate("(Common)", "Signal Lock"),
                "slock", 0, true, 0, 1, 0ms);
        slock.SetValue(1);
        SIGNAL_LOCK<<slock.GetName()<<slock.GetStatus();
    }
}

SignalMonitorValue::SignalMonitorValue(QString _name,
                                       QString _noSpaceName,
                                       int _threshold,
                                       bool _high_threshold,
                                       int _min, int _max,
                                       std::chrono::milliseconds _timeout) :
    m_name(std::move(_name)),
    m_noSpaceName(std::move(_noSpaceName)),
    m_value(0),
    m_threshold(_threshold),
    m_minVal(_min), m_maxVal(_max), m_timeout(_timeout),
    m_highThreshold(_high_threshold)
{
    Init();
#if DEBUG_SIGNAL_MONITOR_VALUE
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("SignalMonitorValue(%1, %2, %3, %4, %5, %6, %7, %8, %9)")
            .arg(m_name, m_noSpaceName) .arg(m_value) .arg(m_threshold)
            .arg(m_minVal) .arg(m_maxVal) .arg(m_timeout.count()) .arg(m_highThreshold)
            .arg((m_set ? "true" : "false")));
#endif
}

SignalMonitorValue::SignalMonitorValue(QString _name,
                                       QString _noSpaceName,
                                       int _value, int _threshold,
                                       bool _high_threshold,
                                       int _min, int _max,
                                       std::chrono::milliseconds _timeout,
                                       bool _set) :
    m_name(std::move(_name)),
    m_noSpaceName(std::move(_noSpaceName)),
    m_value(_value),
    m_threshold(_threshold),
    m_minVal(_min), m_maxVal(_max), m_timeout(_timeout),
    m_highThreshold(_high_threshold), m_set(_set)
{
    Init();
#if DEBUG_SIGNAL_MONITOR_VALUE
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("SignalMonitorValue(%1, %2, %3, %4, %5, %6, %7, %8, %9)")
            .arg(m_name, m_noSpaceName) .arg(m_value) .arg(m_threshold)
            .arg(m_minVal) .arg(m_maxVal) .arg(m_timeout.count()) .arg(m_highThreshold)
            .arg((m_set ? "true" : "false")));
#endif
}

QString SignalMonitorValue::GetName(void) const
{
    if (m_name.isNull())
        return {};

    return m_name;
}

QString SignalMonitorValue::GetShortName(void) const
{
    if (m_noSpaceName.isNull())
        return {};

    return m_noSpaceName;
}

bool SignalMonitorValue::Set(const QString& _name, const QString& _longString)
{
    m_name = _name;
    const QString& longString = _longString;

    if (m_name.isEmpty() || longString.isEmpty())
        return false;

    if (("message" == m_name) || ("error" == m_name))
    {
        SetRange(0, 1);
        SetValue(0);
        SetThreshold( ("message" == m_name) ? 0 : 1, true );
        SetTimeout( ("message" == m_name) ? 0ms : -1ms );
        m_noSpaceName = m_name;
        m_name = longString;

        return true;
    }

    QStringList vals = longString.split(" ", Qt::SkipEmptyParts);
    if (8 != vals.size() || "(null)" == vals[0])
        return false;

    m_noSpaceName = vals[0];
    SetRange(vals[3].toInt(), vals[4].toInt());
    SetValue(vals[1].toInt());
    SetThreshold(vals[2].toInt(), (bool) vals[6].toInt());
    SetTimeout(std::chrono::milliseconds(vals[5].toInt()));

    m_set = (bool) vals[7].toInt();
    return true;
}

SignalMonitorValue* SignalMonitorValue::Create(const QString& _name,
                                               const QString& _longString)
{
    auto *smv = new SignalMonitorValue();
    if (!smv->Set(_name, _longString))
    {
        delete smv;
        return nullptr;
    }
    return smv;
}

/** \fn SignalMonitorValue::Parse(const QStringList&)
 *  \brief Converts a list of strings to SignalMonitorValue classes.
 *  \param slist List of strings to convert.
 */
SignalMonitorList SignalMonitorValue::Parse(const QStringList& slist)
{
    SignalMonitorValue smv;
    SignalMonitorList monitor_list;
    for (int i=0; i+1<slist.size(); i+=2)
    {
#if DEBUG_SIGNAL_MONITOR_VALUE
        LOG(VB_GENERAL, LOG_DEBUG,
            "Parse(" + slist[i] + ", (" + slist[i+1] + "))");
#endif
        if (smv.Set(slist[i], slist[i+1]))
            monitor_list.push_back(smv);
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("SignalMonitorValue::Parse(): Error, "
                        "unable to parse (%1, (%2))")
                    .arg(slist[i], slist[i+1]));
        }
    }
    return monitor_list;
}

/** \fn SignalMonitorValue::AllGood(const SignalMonitorList&)
 *  \brief Returns true if all the values in the list return true on IsGood().
 *  \param slist List of SignalMonitorValue classes to check.
 */
bool SignalMonitorValue::AllGood(const SignalMonitorList& slist)
{
    auto isgood = [](const auto & smv){ return smv.IsGood(); };
    bool good = std::all_of(slist.cbegin(), slist.cend(), isgood);
#if DEBUG_SIGNAL_MONITOR_VALUE
    if (!good)
    {
        QString msg("AllGood failed on ");
        for (const auto & smv : slist)
        {
            if (!smv.IsGood())
            {
                msg += smv.m_noSpaceName;
                msg += QString("(%1%2%3) ")
                           .arg(smv.GetValue())
                           .arg(smv.m_highThreshold ? "<" : ">")
                           .arg(smv.GetThreshold());
            }
        }
        LOG(VB_GENERAL, LOG_DEBUG, msg);
    }
#endif
    return good;
}

/** \fn SignalMonitorValue::MaxWait(const SignalMonitorList&)
 *  \brief Returns the maximum timeout value in the signal monitor list.
 *  \param slist List of SignalMonitorValue classes to check.
 */
std::chrono::milliseconds SignalMonitorValue::MaxWait(const SignalMonitorList& slist)
{
    std::chrono::milliseconds wait = 0ms;
    std::chrono::milliseconds minWait = 0s;
    for (const auto & smv : slist)
    {
        wait = std::max(wait, smv.GetTimeout());
        minWait = std::min(minWait, smv.GetTimeout());
    }
    return (minWait < 0ms) ? -1ms : wait;
}
