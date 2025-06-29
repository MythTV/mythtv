/* -*- myth -*- */
/**
 * @file mythgesture.cpp
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief A C++ ripoff of the stroke library, modified for MythTV.
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
 *
 * This library contains code originally obtained from the libstroke
 * library, which was written by Mark F. Willey.  If I am in offense
 * of any copyright or anything, please let me know and I will make
 * the appropriate fixes.
 */

// Qt
#include <QMetaEnum>

// MythTV
#include "mythgesture.h"

// Std
#include <array>
#include <cmath>
#include <algorithm>
#include <complex>

const QEvent::Type MythGestureEvent::kEventType = static_cast<QEvent::Type>(QEvent::registerEventType());

/*! \class MythGestureEvent
 * \brief A custom event that represents a mouse gesture.
 */

/*! \brief Create a MythGesture
 * \param gesture What type of gesture was performed.
 * \param button The button (if any) that was pressed during the gesture.
 */
MythGestureEvent::MythGestureEvent(Gesture gesture, Qt::MouseButton Button)
  : QEvent(kEventType),
    m_gesture(gesture),
    m_button(Button)
{
}

const std::map<QString, MythGestureEvent::Gesture>MythGesture::kSequences =
{
    { "1",     MythGestureEvent::LongClick },
    { "5",     MythGestureEvent::Click },
    { "456",   MythGestureEvent::Right },
    { "654",   MythGestureEvent::Left  },
    { "258",   MythGestureEvent::Down  },
    { "852",   MythGestureEvent::Up    },
    { "951",   MythGestureEvent::UpLeft    },
    { "753",   MythGestureEvent::UpRight   },
    { "159",   MythGestureEvent::DownRight },
    { "357",   MythGestureEvent::DownLeft  },
    { "96321", MythGestureEvent::UpThenLeft    },
    { "74123", MythGestureEvent::UpThenRight   },
    { "36987", MythGestureEvent::DownThenLeft  },
    { "14789", MythGestureEvent::DownThenRight },
    { "32147", MythGestureEvent::LeftThenDown  },
    { "98741", MythGestureEvent::LeftThenUp    },
    { "12369", MythGestureEvent::RightThenDown },
    { "78963", MythGestureEvent::RightThenUp   },
    { "45654", MythGestureEvent::RightThenLeft },
    { "65456", MythGestureEvent::LeftThenRight },
    { "85258", MythGestureEvent::UpThenDown    },
    { "25852", MythGestureEvent::DownThenUp    }
};

/*! \brief Get the symbolic name of the gesture.
 * \return A string containing the symbolic name of the gesture.
 */
QString MythGestureEvent::GetName() const
{
    return QMetaEnum::fromType<Gesture>().valueToKey(m_gesture);
}

QString MythGestureEvent::GetButtonName() const
{
    return QMetaEnum::fromType<Qt::MouseButtons>().valueToKey(static_cast<int>(m_button));
}

/*! \class MythGesture
 * \brief Contains the points in a stroke, and translates them into
 * gestures.
 *
 * Because the indended use of the stop method is to be called by
 * either the expiration of a timer or when an event is called (or
 * both at the same time) it must have a mutex.
 *
 * \ingroup MythUI_Input
 */

/*! \brief Create a new stroke, specifying tuning values
 * \param max_points The maximum number of points to record.
 * \param min_points The minimum number of points to record.
 * \param max_sequence The maximum producible sequence size.
 * \param scale_ratio The stroke scale ratio
 * \param bin_percent The bin count percentage required
 * to add to the sequence.
 */
MythGesture::MythGesture(size_t MaxPoints, size_t MinPoints,
                         size_t MaxSequence, int ScaleRatio,
                         float BinPercent)
  : m_maxPoints(MaxPoints),
    m_minPoints(MinPoints),
    m_maxSequence(MaxSequence),
    m_scaleRatio(ScaleRatio),
    m_binPercent(BinPercent)
{
}

/*! \brief Adjust horizontal and vertical extremes.
 * \param x The new horizontal extreme.
 * \param y The new vertical extreme
 */
void MythGesture::AdjustExtremes(int X, int Y)
{
    m_minX = std::min(m_minX, X);
    m_maxX = std::max(m_maxX, X);
    m_minY = std::min(m_minY, Y);
    m_maxY = std::max(m_maxY, Y);
}

/*! \brief Determine if the stroke is being recorded.
 * \return True if recording is in progress, otherwise, false.
 */
bool MythGesture::Recording(void)
{
    m_lock.lock();
    bool temp = m_recording;
    m_lock.unlock();
    return temp;
}

/// \brief Start recording
void MythGesture::Start(void)
{
    m_lock.lock();
    m_recording = true;
    m_lock.unlock();
}


/*! \brief Stop recording.
 *
 * This method stores the gesture, as it is, and resets all information.
 */
void MythGesture::Stop(bool Timeout)
{
    m_lock.lock();

    if (m_recording)
    {
        m_recording = false;

        // translate before resetting maximums
        const QString gesture = Translate(Timeout);
        auto found = kSequences.find(gesture);
        if (found != kSequences.cend())
            m_lastGesture = found->second;
        else
            m_lastGesture = MythGestureEvent::Unknown;

        m_minX = m_minY = 10000;
        m_maxX = m_maxY = -1;
    }

    m_lock.unlock();
}


/*! \brief Complete the gesture event of the last completed stroke.
 * \return A new gesture event, or nullptr on error.
 */
MythGestureEvent *MythGesture::GetGesture(void) const
{
    return new MythGestureEvent(m_lastGesture, m_lastButton);
}

/* comments in header */
static int determineBin (QPoint p, int x1, int x2, int y1, int y2)
{
    int bin_num = 1;
    if (p.x() > x1)
        bin_num += 1;
    if (p.x() > x2)
        bin_num += 1;
    if (p.y() > y1)
        bin_num += 3;
    if (p.y() > y2)
        bin_num += 3;

    return bin_num;
}


/*! \brief Translate the stroke into a sequence.
 * \return The sequence string made by the mouse.
 *
 * \note The points will be removed during this method.
 */
QString MythGesture::Translate(bool Timeout)
{
    auto total_points = static_cast<size_t>(m_points.count());

    if (total_points > m_maxPoints)
    {
        m_points.clear();
        return "0";
    }

    /* treat any stroke with less than the minimum number of points as
     * a click (not a drag), which is the center bin */
    if (total_points < m_minPoints)
    {
        m_points.clear();
        if (Timeout)
            return "1";
        return "5";
    }

    QString sequence;

    /* number of bins recorded in the stroke */
    size_t sequence_count = 0;

    /* points-->sequence translation scratch variables */
    int prev_bin = 0;
    int current_bin = 0;
    int bin_count = 0;

    /*flag indicating the start of a stroke - always count it in the sequence*/
    bool first_bin = true;

    /* determine size of grid */
    int delta_x = m_maxX - m_minX;
    int delta_y = m_maxY - m_minY;

    /* calculate bin boundary positions */
    int bound_x_1 = m_minX + (delta_x / 3);
    int bound_x_2 = m_minX + (2 * (delta_x / 3));

    int bound_y_1 = m_minY + (delta_y / 3);
    int bound_y_2 = m_minY + (2 * (delta_y / 3));

    if (delta_x > m_scaleRatio * delta_y)
    {
        bound_y_1 = ((m_maxY + m_minY - delta_x) / 2) + (delta_x / 3);
        bound_y_2 = ((m_maxY + m_minY - delta_x) / 2) + (2 * (delta_x / 3));
    }
    else if (delta_y > m_scaleRatio * delta_x)
    {
        bound_x_1 = ((m_maxX + m_minX - delta_y) / 2) + (delta_y / 3);
        bound_x_2 = ((m_maxX + m_minX - delta_y) / 2) + (2 * (delta_y / 3));
    }

    /* build string by placing points in bins, collapsing bins and
       discarding those with too few points... */

    while (!m_points.empty())
    {
        QPoint pt = m_points.front();
        m_points.pop_front();

        /* figure out which bin the point falls in */
        current_bin = determineBin(pt, bound_x_1, bound_x_2, bound_y_1,
                                   bound_y_2);

        /* if this is the first point, consider it the previous bin, too. */
        prev_bin = (prev_bin == 0) ? current_bin : prev_bin;

        if (prev_bin == current_bin)
            bin_count++;
        else
        {

            /* we are moving to a new bin -- consider adding to the
               sequence */
            if ((bin_count > (total_points * m_binPercent)) || first_bin)
            {
                first_bin = false;
                sequence += '0' + QChar(prev_bin);
                sequence_count ++;
            }

            /* restart counting points in the new bin */
            bin_count = 0;
            prev_bin = current_bin;
        }
    }

    /* add the last run of points to the sequence */
    sequence += '0' + QChar(current_bin);
    sequence_count++;

    /* bail out on error cases */
    if (sequence_count > m_maxSequence)
        sequence = '0';

    return sequence;
}


/*! \brief Determine if the stroke has the minimum required points.
 * \return true if the gesture can be translated, otherwise, false.
 */
bool MythGesture::HasMinimumPoints() const
{
    return static_cast<size_t>(m_points.size()) >= m_minPoints;
}

/*! \brief Record a point.
 * \param Point The point to record.
 * \return True if the point was recorded, otherwise, false.
 */
bool MythGesture::Record(QPoint Point, Qt::MouseButton Button)
{
    /* only record if we haven't exceeded the maximum points */
    if ((static_cast<size_t>(m_points.size()) >= m_maxPoints) || !Recording())
        return false;

    m_lastButton = Button;

    if (m_points.empty())
    {
        m_points.push_back(Point);
        return true;
    }

    /* interpolate between last and current point */
    int delx = Point.x() - m_points.back().x();
    int dely = Point.y() - m_points.back().y();

    /* step by the greatest delta direction */
    if (abs(delx) > abs(dely))
    {
        float fy = m_points.back().y();

        /* go from the last point to the current, whatever direction
         * it may be */
        for (int ix = m_points.back().x();
             (delx > 0) ? (ix < Point.x()) : (ix > Point.x());
             ix += (delx > 0) ? 1 : -1)
        {
            /* step the other axis by the correct increment */
            fy += std::fabs(static_cast<float>(dely) / static_cast<float>(delx))
                * ((dely < 0) ? -1.0F : 1.0F);
            int iy = static_cast<int>(fy);

            /* add the interpolated point */
            m_points.push_back(QPoint(ix, iy));
            AdjustExtremes(ix, iy);
        }
    }
    else /* same thing, but for dely larger than delx case... */
    {
        float fx = m_points.back().x();

        /* go from the last point to the current, whatever direction
           it may be */
        for (int iy = m_points.back().y();
             (dely > 0) ? (iy < Point.y()) : (iy > Point.y());
             iy += (dely > 0) ? 1 : -1)
        {
            /* step the other axis by the correct increment */
            fx += std::fabs(static_cast<float>(delx) / static_cast<float>(dely))
                * ((delx < 0) ? -1.0F : 1.0F);
            int ix = static_cast<int>(fx);

            /* add the interpolated point */
            m_points.push_back(QPoint(ix, iy));
            AdjustExtremes(ix, iy);
        }
    }

    m_points.push_back(Point);

    return true;
}
