/* ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#ifndef ZMDEFINES_H
#define ZMDEFINES_H

#include <utility>
#include <vector>

// qt
#include <QString>
#include <QDateTime>
#include <QTimeZone>

/// Event details
class Event
{
  public:
    Event(int eventID,
          QString eventName,
          int monitorID,
          QString monitorName,
          const QDateTime &startTime,
          QString length) :
        m_monitorID(monitorID),
        m_eventID(eventID),
        m_eventName(std::move(eventName)),
        m_monitorName(std::move(monitorName)),
        m_length(std::move(length)),
        m_startTime(startTime.toLocalTime())
    {
    }

    Event() = default;

    int monitorID(void) const { return m_monitorID; }

    int eventID(void) const { return m_eventID; }

    QString eventName(void) const { return m_eventName; }

    QString monitorName(void) const { return m_monitorName; }

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    /// Returns time using specified Qt::TimeSpec.
    QDateTime startTime(Qt::TimeSpec spec) const
    {
        /// Since the spec will always be a constant Qt::LocalTime,
        /// this just optimizes away to "return m_startTime", but
        /// it remains self documenting and the fact that we keep
        /// the time in this class in local time is encapsulated.
        return (Qt::LocalTime == spec) ?
            m_startTime : m_startTime.toTimeSpec(spec);
    }
#else
    /// Returns time using specified QTimeZone.
    QDateTime startTime(const QTimeZone& zone) const
    {
        return m_startTime.toTimeZone(zone);
    }
#endif

    /// Returns time with native Qt::TimeSpec (subject to revision).
    /// Use only if the code using this functions properly with any timeSpec.
    QDateTime startTime(void) const { return m_startTime; }

    QString length(void) const { return m_length; }

  private:
    int     m_monitorID   { -1 };
    int     m_eventID     { -1 };
    QString m_eventName;
    QString m_monitorName;
    QString m_length;
    /// The start time is stored in local time interally since
    /// all uses are currently using local time and conversion
    /// to/from UTC would consume significant CPU cycles.
    QDateTime m_startTime;
};

enum State : std::uint8_t
{
    IDLE,
    PREALARM,
    ALARM,
    ALERT,
    TAPE
};

// event frame details
struct Frame
{
    QString type;
    double delta {0.0};
};

class Monitor
{
  public:
    Monitor() = default;

  public:
    // used by console view
    int     id                { 0 };
    QString name;
    QString type;
    QString function;
    bool    enabled           { false };
    QString device;
    QString zmcStatus;
    QString zmaStatus;
    int     events            { 0 };
    // used by live view
    QString status;
    int     width             { 0 };
    int     height            { 0 };
    int     bytes_per_pixel   { 0 };
    // used by the alarm notiftications
    bool    showNotifications { false };
    State   state             {  IDLE };
    State   previousState     {  IDLE };
};

Q_DECLARE_METATYPE(Monitor *)

#endif
