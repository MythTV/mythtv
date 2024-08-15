// -*- Mode: c++ -*-
#ifndef SIGNALMONITORVALUES_H
#define SIGNALMONITORVALUES_H

#include <vector>

// Qt headers
#include <QStringList>
#include <QCoreApplication>

#include "libmyth/mythcontext.h"

class SignalMonitorValue
{
    Q_DECLARE_TR_FUNCTIONS(SignalMonitorValue)

    using SignalMonitorList = std::vector<SignalMonitorValue>;
  public:
    SignalMonitorValue(QString _name, QString _noSpaceName,
                       int _threshold, bool _high_threshold,
                       int _min, int _max, std::chrono::milliseconds _timeout);
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
        QString str = m_noSpaceName.isNull() ? "(null)" : m_noSpaceName;
        return QString("%1 %2 %3 %4 %5 %6 %7 %8")
            .arg(str).arg(m_value).arg(m_threshold).arg(m_minVal).arg(m_maxVal)
            .arg(m_timeout.count()).arg((int)m_highThreshold).arg((int)m_set);
    }
    /// \brief Returns the value.
    int GetValue() const { return m_value; }
    /// \brief Returns smallest value possible, used for signal monitor bars.
    int GetMin() const { return m_minVal; }
    /// \brief Returns greatest value possible, used for signal monitor bars.
    int GetMax() const { return m_maxVal; }
    /// \brief Returns the threshold at which the value is considered "good".
    /// \sa IsHighThreshold(), IsGood()
    int GetThreshold() const { return m_threshold; }
    /// \brief Returns true if values greater than the threshold are
    ///        considered good, false otherwise.
    bool IsHighThreshold() const { return m_highThreshold; }
    /// \brief Returns how long to wait for a good value in milliseconds.
    std::chrono::milliseconds GetTimeout() const { return m_timeout; }

    /// \brief Returns true if the value is equal to the threshold, or on the
    ///        right side of the threshold (depends on IsHighThreashold()).
    bool IsGood() const
    {
        return (m_highThreshold) ? m_value >= m_threshold : m_value <= m_threshold;
    }
    /// \brief Returns the value normalized to the [newmin, newmax] range.
    /// \param newmin New minimum value.
    /// \param newmax New maximum value.
    int GetNormalizedValue(int newmin, int newmax) const
    {
        float rangeconv = ((float) (newmax - newmin)) / (GetMax() - GetMin());
        int newval = (int) (((GetValue() - GetMin()) * rangeconv) + newmin);
        return std::clamp(newval, newmin, newmax);
    }


    // // // // // // // // // // // // // // // // // // // // // // // //
    // Sets  // // // // // // // // // // // // // // // // // // // // //

    void SetValue(int _value)
    {
        m_set = true;
        m_value = std::clamp(_value,m_minVal,m_maxVal);
    }

    void SetMin(int _min) { m_minVal = _min; }

    void SetMax(int _max) { m_maxVal = _max; }

    void SetThreshold(int _threshold) { m_threshold = _threshold; }

    void SetThreshold(int _threshold, bool _high_threshold) {
        m_threshold = _threshold;
        m_highThreshold = _high_threshold;
    }

    /// \brief Sets the minimum and maximum values.
    /// \sa SetMin(int), SetMax(int)
    void SetRange(int _min, int _max) {
        m_minVal = _min;
        m_maxVal = _max;
    }

    void SetTimeout(std::chrono::milliseconds _timeout) { m_timeout = _timeout; }

    // // // // // // // // // // // // // // // // // // // // // // // //
    // Static Methods // // // // // // // // // // // // // // // // // //

    static void Init();
    static SignalMonitorValue*
        Create(const QString& _name, const QString& _longString);
    static SignalMonitorList Parse(const QStringList& slist);
    static bool AllGood(const SignalMonitorList& slist);
    static std::chrono::milliseconds MaxWait(const SignalMonitorList& slist);


    // // // // // // // // // // // // // // // // // // // // // // // //
    // Constants   // // // // // // // // // // // // // // // // // // //

    static QStringList ERROR_NO_CHANNEL;
    static QStringList ERROR_NO_LINK;
    static QStringList SIGNAL_LOCK;
    // variable for initializing constants after translator installed
    static bool run_static_init;

    QString toString() const
    {
        QString str = m_noSpaceName.isNull() ? "(null)" : m_noSpaceName;
        return QString("Name(%1) Val(%2) thr(%3%4) range(%5,%6) "
                       "timeout(%7 ms) %8 set. %9 good.")
            .arg(str).arg(m_value).arg( (m_highThreshold) ? ">=" : "<=" )
            .arg(m_threshold).arg(m_minVal).arg(m_maxVal)
            .arg(m_timeout.count())
            .arg(m_set    ? "is" : "is NOT",
                 IsGood() ? "Is" : "Is NOT" );
    }
  private:
    SignalMonitorValue() = default;
    SignalMonitorValue(QString  _name, QString  _noSpaceName,
                       int _value, int _threshold, bool _high_threshold,
                       int _min, int _max, std::chrono::milliseconds _timeout, bool _set);
    bool Set(const QString& _name, const QString& _longString);

    QString m_name;
    QString m_noSpaceName;
    int     m_value         {-1};
    int     m_threshold     {-1};
    int     m_minVal        {-1};
    int     m_maxVal        {-1};
    std::chrono::milliseconds m_timeout {-1ms};
    bool    m_highThreshold {true}; // false when we must be below threshold
    bool    m_set           {false}; // false until value initially set
};

using SignalMonitorList = std::vector<SignalMonitorValue>;

#endif // SIGNALMONITORVALUES_H
