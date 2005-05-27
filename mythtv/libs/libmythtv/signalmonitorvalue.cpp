#include "signalmonitorvalue.h"
#include <qobject.h>

bool SignalMonitorValue::run_static_init = true;
QStringList SignalMonitorValue::ERROR_NO_CHANNEL;
QStringList SignalMonitorValue::ERROR_NO_LINK;
QStringList SignalMonitorValue::SIGNAL_LOCK;

#define DEBUG_SIGNAL_MONITOR_VALUE 0

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
        ERROR_NO_CHANNEL<<"error"<<QObject::tr("Could not open tuner device");
        ERROR_NO_LINK<<"error"<<QObject::tr("Bad connection to backend");

        SignalMonitorValue slock(
            QObject::tr("Signal Lock"), "slock", 0, true, 0, 1, 0);
        slock.SetValue(1);
        SIGNAL_LOCK<<slock.GetName()<<slock.GetStatus();
    }
}

SignalMonitorValue::SignalMonitorValue(const QString& _name,
                                       const QString& _noSpaceName, 
                                       int _threshold,
                                       bool _high_threshold,
                                       int _min, int _max,
                                       int _timeout) :
    name(_name), noSpaceName(_noSpaceName),
    value(0),
    threshold(_threshold),
    minval(_min), maxval(_max), timeout(_timeout),
    high_threshold(_high_threshold), set(false)
{
    Init();
#if DEBUG_SIGNAL_MONITOR_VALUE
    cerr<<"SignalMonitorValue("<<name<<", "<<noSpaceName<<", "<<value<<", "
        <<threshold<<", "<<minval<<", "<<maxval<<", "<<timeout<<", "
        <<high_threshold<<", "<< ((set) ? "true" : "false") <<")"<<endl;
#endif
}

SignalMonitorValue::SignalMonitorValue(const QString& _name,
                                       const QString& _noSpaceName, 
                                       int _value, int _threshold,
                                       bool _high_threshold,
                                       int _min, int _max,
                                       int _timeout, bool _set) :
    name(_name), noSpaceName(_noSpaceName),
    value(_value),
    threshold(_threshold),
    minval(_min), maxval(_max), timeout(_timeout),
    high_threshold(_high_threshold), set(_set)
{
    Init();
#if DEBUG_SIGNAL_MONITOR_VALUE
    cerr<<"SignalMonitorValue("<<name<<", "<<noSpaceName<<", "<<value<<", "
        <<threshold<<", "<<minval<<", "<<maxval<<", "<<timeout<<", "
        <<high_threshold<<", "<< ((set) ? "true" : "false") <<")"<<endl;
#endif
}

bool SignalMonitorValue::Set(const QString& _name, const QString& _longString)
{
    name = _name;
    QString longString = _longString;

    if (QString::null == name || QString::null == longString)
        return false;

    if (("message" == name) || ("error" == name))
    {
        noSpaceName = longString;
        SetValue(0);
        SetRange(0, 1);
        SetThreshold( ("message" == name) ? 0 : 1, true );
        SetTimeout( ("message" == name) ? 0 : -1 );

        return true;
    }

    QStringList vals = QStringList::split(" ", longString);

    if (8 != vals.size() && "(null)" == vals[0])
        return false;

    noSpaceName = vals[0];
    SetValue(vals[1].toInt());
    SetRange(vals[3].toInt(), vals[4].toInt());
    SetThreshold(vals[2].toInt(), (bool) vals[6].toInt());
    SetTimeout(vals[5].toInt());

    set = (bool) vals[7].toInt();

    return true;
}

SignalMonitorValue* SignalMonitorValue::Create(const QString& _name,
                                               const QString& _longString)
{
    SignalMonitorValue *smv = new SignalMonitorValue();
    if (!smv->Set(_name, _longString))
    {
        delete smv;
        return NULL;
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
    for (uint i=0; i+1<slist.size(); i+=2)
    {
#if DEBUG_SIGNAL_MONITOR_VALUE
        cerr<<"Parse("<<slist[i]<<", ("<<slist[i+1]<<"))"<<endl;
#endif
        if (smv.Set(slist[i], slist[i+1]))
            monitor_list.push_back(smv);
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("SignalMonitorValue::Parse(): Error, "
                            "unable to parse (%1, (%2))")
                    .arg(slist[i]).arg(slist[i+1]));
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
    bool good = true;
    SignalMonitorList::const_iterator it = slist.begin();
    for (; it != slist.end(); ++it)
        good &= it->IsGood();
#if DEBUG_SIGNAL_MONITOR_VALUE
    if (!good)
    {
        cerr<<"AllGood failed on ";
        SignalMonitorList::const_iterator it = slist.begin();
        for (; it != slist.end(); ++it)
            if (!it->IsGood())
            {
                cerr<<it->noSpaceName<<"("<<it->GetValue()
                    <<((it->high_threshold) ? "<" : ">")
                    <<it->GetThreshold()<<") ";
            }
        cerr<<endl;
    }
#endif
    return good;
}

/** \fn SignalMonitorValue::MaxWait(const SignalMonitorList&)
 *  \brief Returns the maximum timeout value in the signal monitor list.
 *  \param slist List of SignalMonitorValue classes to check.
 */
int SignalMonitorValue::MaxWait(const SignalMonitorList& slist)
{
    int wait = 0, minWait = 0;
    SignalMonitorList::const_iterator it = slist.begin();
    for (; it != slist.end(); ++it)
    {
        wait = max(wait, it->GetTimeout());
        minWait = min(minWait, it->GetTimeout());
    }
    return (minWait<0) ? -1 : wait;
}

