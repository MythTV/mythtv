// -*- Mode: c++ -*-
#ifndef SIGNALMONITORVALUES_H
#define SIGNALMONITORVALUES_H

#include <vector>
using namespace std;

// Qt headers
#include <QStringList>
#include <QCoreApplication>

#include "mythcontext.h"

class SignalMonitorValue
{
    Q_DECLARE_TR_FUNCTIONS(SignalMonitorValue)

    typedef vector<SignalMonitorValue> SignalMonitorList;
  public:
    SignalMonitorValue(const QString& _name, const QString& _noSpaceName,
                       int _threshold, bool _high_threshold,
                       int _min, int _max, int _timeout);
    virtual ~SignalMonitorValue() { ; } /* forces class to have vtable */

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Gets  // // // // // // // // // // // // // // // // // // // // //

    /// \brief Returns the long name of this value.
    QString GetName(void) const;
    /// \brief Returns a space free name of the value. Used by GetStatus().
    QString GetShortName(void) const;
    /// \brief Returns a signal monitor value as one long string.
    QString GetStatus() const
    {
        QString str = (QString::null == noSpaceName) ? "(null)" : noSpaceName;
        return QString("%1 %2 %3 %4 %5 %6 %7 %8")
            .arg(str).arg(value).arg(threshold).arg(minval).arg(maxval)
            .arg(timeout).arg((int)high_threshold).arg((int)set);
    }
    /// \brief Returns the value.
    int GetValue() const { return value; }
    /// \brief Returns smallest value possible, used for signal monitor bars.
    int GetMin() const { return minval; }
    /// \brief Returns greatest value possible, used for signal monitor bars.
    int GetMax() const { return maxval; }
    /// \brief Returns the threshold at which the value is considered "good".
    /// \sa IsHighThreshold(), IsGood()
    int GetThreshold() const { return threshold; }
    /// \brief Returns true if values greater than the threshold are
    ///        considered good, false otherwise.
    bool IsHighThreshold() const { return high_threshold; }
    /// \brief Returns how long to wait for a good value in milliseconds.
    int GetTimeout() const { return timeout; }

    /// \brief Returns true if the value is equal to the threshold, or on the
    ///        right side of the threshold (depends on IsHighThreashold()).
    bool IsGood() const
    {
        return (high_threshold) ? value >= threshold : value <= threshold;
    }
    /// \brief Returns the value normalized to the [newmin, newmax] range.
    /// \param newmin New minimum value.
    /// \param newmax New maximum value.
    int GetNormalizedValue(int newmin, int newmax) const
    {
        float rangeconv = ((float) (newmax - newmin)) / (GetMax() - GetMin());
        int newval = (int) (((GetValue() - GetMin()) * rangeconv) + newmin);
        return max( min(newval, newmax), newmin );
    }


    // // // // // // // // // // // // // // // // // // // // // // // //
    // Sets  // // // // // // // // // // // // // // // // // // // // //

    void SetValue(int _value)
    {
        set = true;
        value = min(max(_value,minval),maxval);
    }

    void SetMin(int _min) { minval = _min; }

    void SetMax(int _max) { maxval = _max; }

    void SetThreshold(int _threshold) { threshold = _threshold; }

    void SetThreshold(int _threshold, bool _high_threshold) {
        threshold = _threshold;
        high_threshold = _high_threshold;
    }

    /// \brief Sets the minimum and maximum values.
    /// \sa SetMin(int), SetMax(int)
    void SetRange(int _min, int _max) {
        minval = _min;
        maxval = _max;
    }

    void SetTimeout(int _timeout) { timeout = _timeout; }

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Static Methods // // // // // // // // // // // // // // // // // //

    static void Init();
    static SignalMonitorValue*
        Create(const QString& _name, const QString& _longString);
    static SignalMonitorList Parse(const QStringList& list);
    static bool AllGood(const SignalMonitorList& slist);
    static int MaxWait(const SignalMonitorList& slist);


    // // // // // // // // // // // // // // // // // // // // // // // //
    // Constants   // // // // // // // // // // // // // // // // // // //

    static QStringList ERROR_NO_CHANNEL;
    static QStringList ERROR_NO_LINK;
    static QStringList SIGNAL_LOCK;
    // variable for initializing constants after translator installed
    static bool run_static_init;

    QString toString() const
    {
        QString str = (QString::null == noSpaceName) ? "(null)" : noSpaceName;
        return QString("Name(%1) Val(%2) thr(%3%4) range(%5,%6) "
                       "timeout(%7 ms) %8 set. %9 good.")
            .arg(str).arg(value).arg( (high_threshold) ? ">=" : "<=" )
            .arg(threshold).arg(minval).arg(maxval)
            .arg(timeout).arg( (set) ? "is" : "is NOT" )
            .arg( (IsGood()) ? "Is" : "Is NOT" );
    }
  private:
    SignalMonitorValue() :
        value(-1), threshold(-1), minval(-1), maxval(-1), timeout(-1),
        high_threshold(true), set(false) { }
    SignalMonitorValue(const QString& _name, const QString& _noSpaceName,
                       int _value, int _threshold, bool _high_threshold,
                       int _min, int _max, int _timeout, bool _set);
    bool Set(const QString& _name, const QString& _longString);

    QString name;
    QString noSpaceName;
    int     value;
    int     threshold;
    int     minval;
    int     maxval;
    int     timeout;
    bool    high_threshold; // false when we must be below threshold
    bool    set; // false until value initially set
};

typedef vector<SignalMonitorValue> SignalMonitorList;

#endif // SIGNALMONITORVALUES_H
