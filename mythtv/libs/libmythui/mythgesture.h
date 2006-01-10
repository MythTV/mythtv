/* -*- myth -*- */
/**
 * @file mythgesture.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief A C++ ripoff of the stroke library for MythTV.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 */
#ifndef MYTHGESTURE_H
#define MYTHGESTURE_H

#include <sys/types.h>

#include <qpoint.h>
#include <qvaluelist.h>
#include <qevent.h>

const int MythGestureEventType = 24427;

/**
 * @class MythGestureEvent
 * @brief A custom event that represents a mouse gesture.
 */
class MythGestureEvent : public QCustomEvent
{
  public:
    /**
     * @brief The types of gestures supported by myth
     */
    enum Gesture {

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

        /* A click */
        Click,

        /* This isn't real */
        MaxGesture
    };

    /**
     * @brief Create a myth gesture.
     * @param type The gesture type, as per the Type enumeration.
     * @sa Type
     */
    inline MythGestureEvent(size_t gesture):QCustomEvent(MythGestureEventType)
    {
        (gesture >= MaxGesture) ? _gesture = MaxGesture : _gesture = gesture;
    }

    /**
     * @brief Get the gesture type.
     * @return The gesture value corresponding to the Gesture
     * enumeration.
     */
    inline int gesture(void) const { return this->_gesture; }

    /**
     * @brief Get the symbolic name of the gesture.
     * @return A string containing the symbolic name of the gesture.
     */
    operator QString() const;

 private:

    size_t _gesture;
};

/* forward declaration of private information */
class MythGesturePrivate;

/**
 * @class MythGesture
 * @brief Contains the points in a stroke, and translates them into 
 * gestures.
 *
 * Becase the indended use of the stop method is to be called by
 * either the expiration of a timer or when an event is called (or
 * both at the same time) it must have a mutex.
 */
class MythGesture 
{
public:

    /**
     * @brief Create a new stroke, specifying tuning values
     * @param max_points The maximum number of points to record.
     * @param min_points The minimum number of points to record.
     * @param max_sequence The maximum producible sequence size.
     * @param scale_ratio The stroke scale ratio
     * @param bin_percent The bin count percentage required
     * to add to the sequence.
     * @param timeout The milliseconds that pass before a timout will
     * occur.
     */
    MythGesture(size_t max_points = 10000, size_t min_points = 50,
                size_t max_sequence = 20, size_t scale_ratio = 4,
                float bin_percent = 0.07);

    /**
     * @brief Start recording.
     */
    void start(void);

    /**
     * @brief Stop recording.
     *
     * This method stores the gesture, as it is, and resets all
     * information.
     */
    void stop(void);

    /**
     * @brief Determine if the stroke is being recorded.
     * @return True if recording is in progress, otherwise, false.
     */
    bool recording(void) const;

    /**
     * @brief Complete the gesture event of the last completed stroke.
     * @return A new gesture event, or NULL on error.
     */
    MythGestureEvent *gesture(void) const;

    /**
     * @brief Record a point.
     * @param p The point to record.
     * @return True if the point was recorded, otherwise, false.
     */
    bool record(const QPoint &p);

    /**
     * @brief Determine if the stroke has the minimum required points.
     * @return true if the gesture can be translated, otherwise, false.
     */
    bool hasMinimumPoints(void) const { return points.size() >= min_points; }

protected:

    /**
     * @brief Translate the stroke into a sequence.
     * @return The sequence string made by the mouse.
     *
     * @note The points will be removed during this method.
     */
    QString translate(void);

    /**
     * @brief Adjust horizontal and vertical extremes.
     * @param x The new horizontal extreme.
     * @param y The new vertical extreme
     */
    void adjustExtremes(int x, int y);

private:

    bool m_recording;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    size_t max_points;
    size_t min_points;
    size_t max_sequence;
    int scale_ratio;
    float bin_percent;
    size_t last_gesture;    
    QValueList <QPoint> points;

    MythGesturePrivate *p;
};

#endif /* MYTHGESTURE_H */

