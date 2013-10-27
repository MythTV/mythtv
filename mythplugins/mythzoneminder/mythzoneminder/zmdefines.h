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

#include <vector>

// qt
#include <QString>
#include <QDateTime>

/// Event details
class Event
{
  public:
    Event(int eventID,
          const QString &eventName,
          int monitorID,
          const QString &monitorName,
          const QDateTime &startTime,
          const QString &length) :
        m_monitorID(monitorID),
        m_eventID(eventID),
        m_eventName(eventName),
        m_monitorName(monitorName),
        m_length(length),
        m_startTime(startTime.toLocalTime())
    {
    }

    Event() : m_monitorID(-1), m_eventID(-1) {}

    int monitorID(void) const { return m_monitorID; }

    int eventID(void) const { return m_eventID; }

    QString eventName(void) const { return m_eventName; }

    QString monitorName(void) const { return m_monitorName; }

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

    /// Returns time with native Qt::TimeSpec (subject to revision).
    /// Use only if the code using this functions properly with any timeSpec.
    QDateTime startTime(void) const { return m_startTime; }

    QString length(void) const { return m_length; }

  private:
    int m_monitorID;
    int m_eventID;
    QString m_eventName;
    QString m_monitorName;
    QString m_length;
    /// The start time is stored in local time interally since
    /// all uses are currently using local time and conversion
    /// to/from UTC would consume significant CPU cycles.
    QDateTime m_startTime;
};

// event frame details
typedef struct
{
    QString type;
    double delta;
} Frame;

class Monitor
{
  public:
    Monitor() :
        id(0), enabled(0), events(0),
        width(0), height(0), bytes_per_pixel(0)
    {
    }

  public:
    // used by console view
    int     id;
    QString name;
    QString type;
    QString function;
    int enabled;
    QString device;
    QString zmcStatus;
    QString zmaStatus;
    int events;
    // used by live view
    QString status;
    int width;
    int height;
    int bytes_per_pixel;
};

#endif
