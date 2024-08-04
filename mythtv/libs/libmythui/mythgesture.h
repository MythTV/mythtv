/**
 * \file mythgesture.h
 * \author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * \brief A C++ ripoff of the stroke library for MythTV.
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
#ifndef MYTHGESTURE_H
#define MYTHGESTURE_H

// Qt
#include <QPoint>
#include <QList>
#include <QString>
#include <QEvent>
#include <QMutex>

// MythTV
#include "mythuiexp.h"

// Std
#include <sys/types.h>
#include <map>

class MUI_PUBLIC MythGestureEvent : public QEvent
{
    Q_GADGET

  public:
    enum Gesture : std::uint8_t
    {
        Unknown,

        /* Horizontal and vertical lines */
        Up,
        Down,
        Left,
        Right,

        /* Diagonal lines */
        UpLeft,
        UpRight,
        DownLeft,
        DownRight,

        /* Two Lines */
        UpThenLeft,
        UpThenRight,
        DownThenLeft,
        DownThenRight,
        LeftThenUp,
        LeftThenDown,
        RightThenUp,
        RightThenDown,
        RightThenLeft,
        LeftThenRight,
        UpThenDown,
        DownThenUp,

        /* A click */
        // NB LongClick before Click as MythMainWindow filters out otherwise
        LongClick,
        Click
    };
    Q_ENUM(Gesture)

    explicit MythGestureEvent(Gesture gesture, Qt::MouseButton Button);
    ~MythGestureEvent() override = default;

    QString         GetName     () const;
    Gesture         GetGesture  () const { return m_gesture; }
    void            SetPosition (QPoint Position) { m_position = Position; }
    QPoint          GetPosition () const { return m_position; }
    Qt::MouseButton GetButton   () const { return m_button; }
    QString         GetButtonName() const;

    static const Type kEventType;

  private:
    Gesture m_gesture { Unknown  };
    QPoint  m_position;
    Qt::MouseButton m_button { Qt::NoButton };
};

class MythGesture
{
  public:
    explicit MythGesture(size_t MaxPoints = 10000, size_t MinPoints = 50,
                         size_t MaxSequence = 20,  int ScaleRatio = 4,
                         float BinPercent = 0.07F);

    void Start();
    void Stop(bool Timeout = false);
    bool Recording();
    MythGestureEvent* GetGesture() const;
    bool Record(QPoint Point, Qt::MouseButton Button);

  private:
    Q_DISABLE_COPY(MythGesture)

    bool    HasMinimumPoints() const;
    QString Translate(bool Timeout);
    void    AdjustExtremes(int X, int Y);

    bool   m_recording    { false };
    int    m_minX         { 10000 };
    int    m_maxX         { -1    };
    int    m_minY         { 10000 };
    int    m_maxY         { -1    };
    size_t m_maxPoints    { 10000 };
    size_t m_minPoints    { 50    };
    size_t m_maxSequence  { 20    };
    int    m_scaleRatio   { 4     };
    float  m_binPercent   { 0.07F };
    MythGestureEvent::Gesture m_lastGesture { MythGestureEvent::Unknown };
    QList <QPoint> m_points;
    QMutex m_lock;
    static const std::map<QString, MythGestureEvent::Gesture> kSequences;
    Qt::MouseButton m_lastButton { Qt::NoButton };
};

#endif

